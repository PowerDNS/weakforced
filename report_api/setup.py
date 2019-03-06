import os
from setuptools import setup, find_packages

install_reqs = list()

# Use pipenv for dependencies, setuptools otherwise.
# This makes the installation for the packages easier (no pipenv needed)
try:
    from pipenv.project import Project
    from pipenv.utils import convert_deps_to_pip
    pfile = Project(chdir=False).parsed_pipfile
    install_reqs = convert_deps_to_pip(pfile['packages'], r=False)
except ImportError:
    try:
        from pip.req import parse_requirements
    except ImportError:
        from pip._internal.req import parse_requirements
    install_reqs = [str(ir.req) for ir in parse_requirements(
        './requirements.txt', session=False)]


def exists(fname):
    return os.path.exists(os.path.join(os.path.dirname(__file__), fname))


# Utility function to read the README file.
# Used for the long_description.  It's nice, because now 1) we have a top level
# README file and 2) it's easier to type in the README file than to put a raw
# string in below ...
def read(fname):
    return open(os.path.join(os.path.dirname(__file__), fname),
                'r', encoding='utf-8').read()


version = os.environ.get('BUILDER_VERSION', '0.0.0')

if exists('version.txt'):
    version = read('version.txt').strip()

setup(
    name = "wforce-report-api",
    version = version,
    author = "Neil Cook",
    author_email = "",
    description = (""),
    license = "GPL 3",
    keywords = "PowerDNS Zonecontrol",
    url = "https://github.com/PowerDNS/weakforced",
    packages = find_packages(),
    install_requires=install_reqs,
    include_package_data = True,
    scripts=['runreport.py'],
    long_description="TODO",
    classifiers=[],
)

