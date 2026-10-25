"""Compile the actual color-gating code against a standard-API-only fixture."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which('g++'), 'g++ is required for the C++ behavior test')
class StandardColorProcessingTest(unittest.TestCase):
    def test_run_discards_frames_and_calibration_registers_color(self):
        source = (ROOT/'src/KinectProjector/KinectGrabber.cpp').read_text()
        setter = source[source.index('void KinectGrabber::setColorProcessingEnabled'):]
        setter = setter[:setter.index('\n}\n')+3]
        frames = source[source.index('    // RUN does not need color registration,'):]
        frames = frames[:frames.index('\nfloat* depthData')]
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            setter = setter.replace('/tmp/magicsand-run-marker.log', str(directory/'marker.log'))
            fixture = r'''
#include <cassert>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
std::ostringstream logger;
std::ostream& ofLogNotice(const char*) { return logger; }
struct KinectGrabber {
    bool processColorFrames = true;
    void setColorProcessingEnabled(bool);
};
namespace libfreenect2 { struct Frame { enum { Color=1 }; }; }
using FrameMap = std::map<int, libfreenect2::Frame*>;
struct Listener {
    bool available;
    int waits=0, releases=0, timeout=-1;
    libfreenect2::Frame frame;
    bool hasNewFrame() const { return available; }
    bool waitForNewFrame(FrameMap& frames, int ms) {
        ++waits; timeout=ms;
        if (!available) return false;
        available=false; frames[libfreenect2::Frame::Color]=&frame; return true;
    }
    void release(FrameMap&) { ++releases; }
};
struct Registration {
    int applies=0, depths=0;
    void apply(libfreenect2::Frame*, libfreenect2::Frame*, libfreenect2::Frame*, libfreenect2::Frame*) { ++applies; }
    void undistortDepth(libfreenect2::Frame*, libfreenect2::Frame*) { ++depths; }
};
''' + setter + r'''
void test_frames(bool processColorFrames, bool available) {
    Listener listener; listener.available=available;
    Listener* colorListener=&listener;
    Registration reg; Registration* registration=&reg;
    FrameMap colorFrames;
    libfreenect2::Frame f;
    libfreenect2::Frame *colorFrame=nullptr, *depthFrame=&f, *undistortedFrame=&f, *registeredFrame=&f;
    bool gotColorThisLoop=false;
''' + frames + r'''
    assert(listener.waits==(!processColorFrames && !available ? 0 : 1));
    assert(listener.timeout==(!processColorFrames && !available ? -1 : processColorFrames ? 50 : 0));
    assert(gotColorThisLoop==(processColorFrames && available));
    assert(reg.applies==(processColorFrames && available ? 1 : 0));
    assert(reg.depths==(processColorFrames && available ? 0 : 1));
    assert(listener.releases==(!processColorFrames && available ? 1 : 0));
    assert(colorFrames.empty()==!(processColorFrames && available));
}
int main() {
    KinectGrabber grabber;
    grabber.setColorProcessingEnabled(false);
    assert(!grabber.processColorFrames);
    grabber.setColorProcessingEnabled(false);
    grabber.setColorProcessingEnabled(true);
    assert(grabber.processColorFrames);
    test_frames(false, true); test_frames(false, false);
    test_frames(true, true); test_frames(true, false);
    assert(logger.str().find("standard driver RGB decoding remains active")!=std::string::npos);
}
'''
            cpp = directory/'test.cpp'
            cpp.write_text(fixture)
            exe = directory/'test'
            subprocess.run(['g++', '-std=c++14', '-Wall', '-Wextra', str(cpp), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True, timeout=10)


if __name__ == '__main__':
    unittest.main()
