#!/usr/bin/env python3
"""Original deterministic effects. No recorded or sampled audio is imported.
Requires numpy and scipy only for regeneration; shipped WAVs need no Python.
"""
from pathlib import Path
import wave
import numpy as np
from scipy.signal import butter, sosfilt
SR=22050
ROOT=Path(__file__).resolve().parent/'payload/bin/data'
rng=np.random.default_rng(3902026)
def noise(n,low,high):
    return sosfilt(butter(3,[low,high],fs=SR,btype='bandpass',output='sos'),rng.normal(size=n))
def fade(x,seconds=.15):
    n=min(int(SR*seconds),len(x)//2);x[:n]*=np.linspace(0,1,n);x[-n:]*=np.linspace(1,0,n);return x
def write(name,x):
    x=sosfilt(butter(2,24,fs=SR,btype='highpass',output='sos'),x)
    x=fade(x,.12);x*=.82/max(float(np.max(np.abs(x))),1.e-9)
    out=ROOT/name;out.parent.mkdir(parents=True,exist_ok=True)
    with wave.open(str(out),'wb') as f:
        f.setparams((1,2,SR,len(x),'NONE','not compressed'))
        f.writeframes(np.round(x*32767).astype('<i2').tobytes())
# Thirty-second storm: first main flash occurs one second after audio starts;
# additional flash landmarks are 9,21,26 seconds into the recording.
t=np.arange(30*SR)/SR
thunder=np.zeros_like(t)
for onset,amount,decay in [(1,1,6),(9,.65,7),(21,.72,5),(26,.52,3.5)]:
    age=np.maximum(0,t-onset);active=(t>=onset)
    swell=(1-np.exp(-age*24))*np.exp(-age/decay)*active
    rolling=.65+.35*np.sin(2*np.pi*(.47*t+.03*np.sin(.83*t)))**2
    body=noise(len(t),30,210)*swell*rolling
    crack=noise(len(t),180,1800)*np.exp(-age/.17)*active*(1-np.exp(-age*170))
    thunder+=amount*(body*2.3+crack*.4)
write('rain/thunder.wav',thunder)
# Five-second warning, compatible with the existing countdown sequence.
t=np.arange(5*SR)/SR;warning=np.zeros_like(t)
for k in range(5):
    age=t-k;duration=.20 if k<4 else .38
    mask=(age>=0)&(age<duration)
    env=np.zeros_like(t);env[mask]=np.sin(np.pi*age[mask]/duration)**2
    warning+=env*(np.sin(2*np.pi*(460+55*k)*t)+.15*np.sin(2*np.pi*2*(460+55*k)*t))
write('volcano/warning.wav',warning)
# Thirty-second low-frequency eruption, strongest at onset, rolling aftershocks.
t=np.arange(30*SR)/SR
low=noise(len(t),28,180);mid=noise(len(t),100,650)
env=(1-np.exp(-t*15))*np.exp(-t/10)
for start,size in [(4,.5),(9,.42),(16,.32),(23,.22)]:
    age=np.maximum(0,t-start);env+=size*(1-np.exp(-age*5))*np.exp(-age/3)*(t>=start)
rumble=(low*3+mid*.28)*env*(.76+.24*np.sin(2*np.pi*.62*t)**2)
rumble+=.13*np.sin(2*np.pi*(58*t-12*(1-np.exp(-t/5))))*env
write('volcano/eruption.wav',rumble)
print('Created original thunder (30s), countdown (5s), eruption (30s).')
