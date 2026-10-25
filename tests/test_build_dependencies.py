"""Dependency checks must stop early and leave the checkout untouched."""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[1] / 'scripts'
sys.path.insert(0, str(SCRIPTS))
spec = importlib.util.spec_from_file_location('dependency_check', SCRIPTS/'check-dependencies.py')
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)
from configure import configure


class DependencyCheckTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.project = self.root/'project'
        self.project.mkdir()
        self.of = self.root/'of'
        mk = self.of/'libs/openFrameworksCompiled/project/makefileCommon/compile.project.mk'
        mk.parent.mkdir(parents=True)
        mk.write_text('# fixture')
        self.names = ['ofxCv','ofxDatGui','ofxModal','ofxParagraph','ofxOpenCv','ofxXmlSettings']
        (self.project/'addons.make').write_text('\n'.join(self.names))
        for name in self.names:
            src = self.of/'addons'/name/'src'
            src.mkdir(parents=True)
            (src/'addon.h').write_text('// fixture')
        self.kinect = self.root/'libfreenect2'
        for name in ['include/libfreenect2/libfreenect2.hpp','build/libfreenect2/config.h','build/lib/libfreenect2.so']:
            p = self.kinect/name
            p.parent.mkdir(parents=True,exist_ok=True)
            p.write_text('// fixture')
        self.paths = dict(of_root=str(self.of), freenect_root=str(self.kinect), freenect_build=None, freenect_lib=None)

    def call(self, exists=lambda name: True, tools=lambda name: '/usr/bin/'+name):
        output = io.StringIO()
        with patch.object(check.shutil,'which',side_effect=tools), patch.object(check,'pkg_exists',side_effect=exists), patch.object(check,'run',return_value=None), contextlib.redirect_stdout(output):
            code = check.check(self.project,self.paths)
        return code,output.getvalue()

    def test_success_is_read_only(self):
        code,out = self.call()
        self.assertEqual(code,0,out)
        self.assertIn('PASS:',out)
        self.assertFalse((self.project/'config.local.make').exists())
        self.assertFalse((self.project/'topoplay-build.json').exists())

    def test_missing_addon_and_system_library_reported_together(self):
        (self.of/'addons/ofxDatGui/src/addon.h').unlink()
        code,out = self.call(exists=lambda name:name not in ('opencv4','opencv'))
        self.assertEqual(code,1)
        self.assertIn('ofxDatGui',out)
        self.assertIn('libopencv-dev',out)
        self.assertIn('No build started',out)

    def test_missing_pkg_config_explains_unchecked_libraries(self):
        code,out = self.call(tools=lambda name:None if name=='pkg-config' else '/usr/bin/'+name)
        self.assertEqual(code,1)
        self.assertIn('sudo apt install pkg-config',out)
        self.assertIn('could not run',out)

    def test_opencv_legacy_pkg_config_name_accepted(self):
        code,out = self.call(exists=lambda name:name!='opencv4')
        self.assertEqual(code,0,out)

    def test_missing_kinect_still_reports_addons(self):
        (self.kinect/'build/lib/libfreenect2.so').unlink()
        (self.of/'addons/ofxCv/src/addon.h').unlink()
        code,out = self.call()
        self.assertEqual(code,1)
        self.assertIn('libfreenect2',out)
        self.assertIn('ofxCv',out)

    def test_configure_still_writes_explicit_paths(self):
        with contextlib.redirect_stdout(io.StringIO()):
            values = configure(self.project,**self.paths)
        self.assertEqual(values['of_root'],str(self.of))
        self.assertIn('FREENECT2_LIB_DIR := '+str(self.kinect/'build/lib'),(self.project/'config.local.make').read_text())

    def test_build_stops_before_configuring_or_calling_make(self):
        scripts = self.project/'scripts'
        scripts.mkdir()
        import shutil
        for name in ('build.sh', 'check-dependencies.py', 'configure.py', 'topoplay_paths.py'):
            shutil.copy2(SCRIPTS/name, scripts/name)
        tools = self.root/'tools'
        tools.mkdir()
        marker = self.root/'make-was-called'
        fake_make = tools/'make'
        fake_make.write_text('#!/bin/sh\ntouch '+str(marker)+'\n')
        fake_make.chmod(0o755)
        result = subprocess.run(
            ['bash', str(scripts/'build.sh'), '--of-root', str(self.root/'missing-framework')],
            env={**os.environ, 'PATH':str(tools)+os.pathsep+os.environ['PATH']},
            capture_output=True, text=True, timeout=30)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('STOP: dependencies need attention', result.stdout)
        self.assertFalse(marker.exists())
        self.assertFalse((self.project/'config.local.make').exists())
        self.assertFalse((self.project/'topoplay-build.json').exists())

if __name__=='__main__':
    unittest.main()
