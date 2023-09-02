# Copyright (c) 2013 by Gilbert Ramirez <gram@alumni.rice.edu>
#
# SPDX-License-Identifier: GPL-2.0-or-later

import subprocesstest
from subprocesstest import count_output, grep_output
import pytest


@pytest.fixture
def dfilter_cmd(cmd_tshark, capture_file, request):
    def wrapped(dfilter, frame_number=None, prefs=None, read_filter=False):
        cmd = [
            cmd_tshark,
            "-n",       # No name resolution
            "-r",       # Next arg is trace file to read
            capture_file(request.instance.trace_file),
        ]
        if frame_number:
            cmd.extend([
                "-2",       # two-pass mode
                "--selected-frame={}".format(frame_number)
            ])
        if read_filter:
            cmd.extend([
                "-2",       # two-pass mode
                "-R",       # read filter (requires two-pass mode)
                dfilter
            ])
        else:
            cmd.extend([
                "-Y",       # packet display filter (used to be -R)
                dfilter
            ])
        if prefs:
            cmd.extend([
            "-o",
            prefs
        ])
        return cmd
    return wrapped

@pytest.fixture(scope='session')
def cmd_dftest(program):
    return program('dftest')


@pytest.fixture
def checkDFilterCount(dfilter_cmd, base_env):
    def checkDFilterCount_real(dfilter, expected_count, prefs=None):
        """Run a display filter and expect a certain number of packets."""
        proc = subprocesstest.check_run(dfilter_cmd(dfilter, prefs=prefs),
                                         capture_output=True,
                                         universal_newlines=True,
                                         env=base_env)
        assert count_output(proc.stdout) == expected_count
    return checkDFilterCount_real

@pytest.fixture
def checkDFilterCountWithSelectedFrame(dfilter_cmd, base_env):
    def checkDFilterCount_real(dfilter, expected_count, selected_frame, prefs=None):
        """Run a display filter and expect a certain number of packets."""
        proc = subprocesstest.check_run(dfilter_cmd(dfilter, frame_number=selected_frame, prefs=prefs),
                                         capture_output=True,
                                         universal_newlines=True,
                                         env=base_env)
        assert count_output(proc.stdout) == expected_count
    return checkDFilterCount_real

@pytest.fixture
def checkDFilterCountReadFilter(dfilter_cmd, base_env):
    def checkDFilterCount_real(dfilter, expected_count):
        """Run a read filter in two pass mode and expect a certain number of packets."""
        proc = subprocesstest.check_run(dfilter_cmd(dfilter, read_filter=True),
                                         capture_output=True,
                                         universal_newlines=True,
                                         env=base_env)
        assert count_output(proc.stdout) == expected_count
    return checkDFilterCount_real

@pytest.fixture
def checkDFilterFail(cmd_dftest, base_env):
    def checkDFilterFail_real(dfilter, error_message):
        """Run a display filter and expect dftest to fail."""
        proc = subprocesstest.run([cmd_dftest, '--', dfilter],
                                capture_output=True,
                                universal_newlines=True,
                                env=base_env)
        assert proc.returncode == 4
        assert error_message in proc.stderr
    return checkDFilterFail_real

@pytest.fixture
def checkDFilterSucceed(cmd_dftest, base_env):
    def checkDFilterSucceed_real(dfilter, expect_stdout=None):
        """Run a display filter and expect dftest to succeed."""
        proc = subprocesstest.run([cmd_dftest, '--', dfilter],
                                capture_output=True,
                                universal_newlines=True,
                                env=base_env)
        assert proc.returncode == 0
        if expect_stdout:
            assert expect_stdout in proc.stdout
    return checkDFilterSucceed_real
