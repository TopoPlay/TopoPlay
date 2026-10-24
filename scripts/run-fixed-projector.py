#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Keep a projector's native GNOME/Wayland mode while TopoPlay runs."""
from pathlib import Path
import argparse,json,os,re,signal,subprocess,sys,time
from topoplay_paths import root,executable,environment
DEFAULT=root()
POLL_SECONDS=1.0
RETRY_SECONDS=3.0
APPLY_SIGNATURE='(uua(iiduba(ssa{sv}))a{sv})'
SERVICE='org.gnome.Mutter.DisplayConfig'
OBJECT='/org/gnome/Mutter/DisplayConfig'

def plain(value):
    return value.unpack() if hasattr(value,'unpack') else value

def prop(values,name,default=None):
    return plain(values.get(name,default))

def current_mode(monitor):
    return next((m for m in monitor[1] if prop(m[6],'is-current',False)),None)

def make_plan(state,output,width,height,initial=False):
    """Preserve all active logical monitors; change only the target mode ID.

    These tuples match Mutter's documented GetCurrentState / ApplyMonitorsConfig
    API. A fresh serial is used on every attempt so lid topology races are retried.
    """
    serial,monitors,logical,properties=state
    physical={m[0][0]:m for m in monitors}
    target=physical.get(output)
    active=any(any(s[0]==output for s in group[5]) for group in logical)
    if not target or not active:
        if initial:raise RuntimeError('Projector '+output+' is not active in GNOME. Available: '+', '.join(physical))
        return None
    old=current_mode(target)
    if old is None:raise RuntimeError('GNOME did not report a current mode for '+output)
    if old[1:3]==(width,height) or list(old[1:3])==[width,height]:return None
    candidates=[m for m in target[1] if m[1]==width and m[2]==height and not prop(m[6],'is-interlaced',False)]
    if not candidates:raise RuntimeError('GNOME does not advertise '+str(width)+'x'+str(height)+' for '+output)
    wanted=min(candidates,key=lambda m:(abs(m[3]-old[3]),not prop(m[6],'is-preferred',False)))
    configuration=[]
    for group in logical:
        x,y,scale,transform,primary,specs,_=group
        attached=[]
        for spec in specs:
            connector=spec[0];monitor=physical[connector]
            mode=wanted if connector==output else current_mode(monitor)
            if mode is None:raise RuntimeError('Current mode missing for active output '+connector)
            if connector==output and not any(abs(float(scale)-float(s))<0.0001 for s in mode[5]):
                raise RuntimeError('The 720p mode cannot retain the current projector scaling; set its scale to 100% in Displays first.')
            if len(specs)>1 and connector==output:
                raise RuntimeError('Projector is in a mirrored group; use Join Displays before locking its independent mode.')
            keep={}
            if 'color-mode' in monitor[2]:keep['color-mode']=('u',int(prop(monitor[2],'color-mode')))
            if 'is-underscanning' in monitor[2]:keep['underscanning']=('b',bool(prop(monitor[2],'is-underscanning')))
            attached.append((connector,mode[0],keep))
        configuration.append((x,y,scale,transform,primary,attached))
    keep_global={}
    if prop(properties,'supports-changing-layout-mode',False) and 'layout-mode' in properties:
        keep_global['layout-mode']=('u',int(prop(properties,'layout-mode')))
    leased=[m[0] for m in monitors if prop(m[2],'is-for-lease',False)]
    if leased:keep_global['monitors-for-lease']=('a(ssss)',leased)
    return serial,configuration,keep_global,old[0],wanted[0]

class ModeLock:
    def __init__(self,output,width,height):
        try:
            from gi.repository import Gio,GLib
            self.Gio,self.GLib=Gio,GLib
            self.bus=Gio.bus_get_sync(Gio.BusType.SESSION,None)
        except ImportError:raise RuntimeError('System Python needs python3-gi and gir1.2-glib-2.0. Install those Ubuntu packages, then rerun this launcher.')
        except Exception as exc:raise RuntimeError('Cannot connect to GNOME session bus: '+str(exc)) from exc
        self.output,self.width,self.height=output,width,height
        self.next_attempt=0;self.next_warning=0;self.observed=None
    def call(self,method,parameters=None):
        try:return self.bus.call_sync(SERVICE,OBJECT,SERVICE,method,parameters,None,self.Gio.DBusCallFlags.NONE,5000,None)
        except Exception as exc:raise RuntimeError('GNOME '+method+': '+str(exc)) from exc
    def state(self):return self.call('GetCurrentState').unpack()
    def status(self):
        state=self.state()
        for monitor in state[1]:
            mode=current_mode(monitor)
            print('[PROJECTOR MODE V51] GNOME '+monitor[0][0]+' current='+(mode[0] if mode else 'inactive'),flush=True)
        return state
    def check(self,initial=False):
        state=self.state()
        target=next((m for m in state[1] if m[0][0]==self.output),None)
        mode=current_mode(target) if target else None
        observed=mode[0] if mode else 'inactive'
        if observed!=self.observed:
            print('[PROJECTOR MODE V51] GNOME native '+self.output+' current='+observed,flush=True);self.observed=observed
        plan=make_plan(state,self.output,self.width,self.height,initial)
        if plan is None:return
        now=time.monotonic()
        if not initial and now<self.next_attempt:return
        self.next_attempt=now+RETRY_SECONDS
        serial,configuration,properties,old,wanted=plan
        # a{sv} values must be typed GVariants, not plain Python values.
        def typed(values):return {k:self.GLib.Variant(t,v) for k,(t,v) in values.items()}
        logical=[(x,y,scale,transform,primary,[(conn,mode,typed(props)) for conn,mode,props in outputs]) for x,y,scale,transform,primary,outputs in configuration]
        params=self.GLib.Variant(APPLY_SIGNATURE,(serial,1,logical,typed(properties)))
        # Method 1 is temporary. Do not write monitors.xml, alter autostart,
        # change another display's resolution, or trigger a save-layout dialog.
        self.call('ApplyMonitorsConfig',params)
        print('[PROJECTOR MODE V51] GNOME restored '+self.output+': '+old+' -> '+wanted,flush=True)

def main(argv=None):
    a=argparse.ArgumentParser(description=__doc__);a.add_argument('--project',type=Path,default=DEFAULT);a.add_argument('--output',default='HDMI-1');a.add_argument('--mode',default='1280x720');a.add_argument('--status',action='store_true');a.add_argument('--no-mode-lock',action='store_true',help='Launch without GNOME mode control on other desktops.');args=a.parse_args(argv)
    if not re.fullmatch(r'[A-Za-z0-9_.-]+',args.output) or not re.fullmatch(r'\d+x\d+',args.mode):raise RuntimeError('Invalid output or mode.')
    width,height=map(int,args.mode.split('x'));lock=None if args.no_mode_lock else ModeLock(args.output,width,height)
    if args.status:
        if lock is None:raise RuntimeError("--status requires GNOME mode control.")
        lock.status();return 0
    project=args.project.expanduser().resolve()
    exe=executable(project)
    if lock:lock.check(initial=True)
    env=environment(project)
    if lock:print('[PROJECTOR MODE V51] Using GNOME native display controls; watching '+args.output+' for '+args.mode,flush=True)
    app=subprocess.Popen([str(exe)],cwd=project,env=env)
    try:
        while app.poll() is None:
            try:
                if lock:lock.check()
            except RuntimeError as exc:
                now=time.monotonic()
                if now>=lock.next_warning:print('[PROJECTOR MODE V51] '+str(exc),flush=True);lock.next_warning=now+15
            time.sleep(POLL_SECONDS)
        return app.returncode
    except KeyboardInterrupt:return 130
    finally:
        if app.poll() is None:
            app.send_signal(signal.SIGINT)
            try:app.wait(timeout=8)
            except subprocess.TimeoutExpired:
                app.terminate()
                try:app.wait(timeout=4)
                except subprocess.TimeoutExpired:app.kill();app.wait()
if __name__=='__main__':
    try:sys.exit(main())
    except (RuntimeError,OSError) as exc:sys.exit('STOP: '+str(exc))
