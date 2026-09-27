"""Contract tests for the required-coverage gate; no display is needed."""
import unittest
import xml.etree.ElementTree as ET

from run_required_graphics import coverage_failures


class RequiredGraphicsTests(unittest.TestCase):
    def test_all_required_cases_must_be_present(self):
        failures = coverage_failures(ET.fromstring('<testsuite/>'), ['gpu'])
        self.assertEqual(failures['Missing'], ['gpu'])

    def test_skips_fail_the_lane(self):
        failures = coverage_failures(ET.fromstring(
            '<testsuite><testcase name="gpu"><skipped/></testcase></testsuite>'), ['gpu'])
        self.assertEqual(failures['Skipped'], ['gpu'])

    def test_failures_and_errors_fail_the_lane(self):
        for tag in ['failure', 'error']:
            failures = coverage_failures(ET.fromstring(
                f'<testsuite><testcase name="gpu"><{tag}/></testcase></testsuite>'), ['gpu'])
            self.assertEqual(failures['Failed'], ['gpu'])

    def test_only_executed_success_is_green(self):
        failures = coverage_failures(ET.fromstring(
            '<testsuite><testcase name="gpu"/></testsuite>'), ['gpu'])
        self.assertFalse(any(failures.values()))


if __name__ == '__main__':
    unittest.main()
