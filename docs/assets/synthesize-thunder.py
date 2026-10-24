#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Generate the public thunder WAV without imported audio. Needs numpy/scipy.
Use: python3 docs/assets/synthesize-thunder.py --output /chosen/thunder.wav
The published asset is reproducible in the recorded numpy/scipy environment;
other library versions can produce slightly different floating-point rounding.
"""
from pathlib import Path
import argparse, wave
import numpy as np
from scipy.signal import butter, sosfilt

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--output',type=Path,required=True)
    args=a.parse_args()
    sr=44100; frames=1539072
    t=np.arange(frames,dtype=np.float64)/sr
    rng=np.random.default_rng(20261023)
    def band(lo,hi):
        return sosfilt(butter(3,[lo,hi],fs=sr,btype='bandpass',output='sos'),rng.normal(size=frames))
    body=band(28,240);crack=band(180,2400);rolling=band(25,110)
    x=np.zeros(frames,dtype=np.float64)
    for onset,gain,decay in [(1.,1.,5.5),(9.,.72,6.),(21.,.82,4.5),(26.,.62,4.)]:
        age=np.maximum(0.,t-onset);active=t>=onset
        swell=(1.-np.exp(-age*28.))*np.exp(-age/decay)*active
        modulation=.70+.30*np.sin(2.*np.pi*(.41*age+.027*np.sin(.81*age)))**2
        x+=gain*(2.8*body*swell*modulation+.48*crack*(1.-np.exp(-age*180.))*np.exp(-age/.18)*active+.6*rolling*swell)
    fade=min(sr,frames//2);x[-fade:]*=np.linspace(1.,0.,fade)
    x*=.82/max(float(np.max(np.abs(x))),1.e-12)
    pcm=np.round(x*32767.).astype('<i2');stereo=np.column_stack((pcm,pcm)).astype('<i2')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with wave.open(str(args.output),'wb') as f:
        f.setparams((2,2,sr,frames,'NONE','not compressed'));f.writeframes(stereo.tobytes())
    print('Generated thunder:',args.output)
if __name__=='__main__':main()
