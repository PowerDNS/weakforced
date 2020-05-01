import os
from setuptools import setup, find_packages
from pkg_resources import parse_requirements
import pathlib

install_reqs = list()

with pathlib.Path('requirements.txt').open() as requirements_txt:
    install_reqs = [
        str(r)
        for r
        in parse_requirements(requirements_txt)]


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
    author_email = "neil.cook@open-xchange.com",
    description = ("Enable access to the report information stored in Elasticsearch."),
    license = "GPL 3",
    keywords = "PowerDNS Zonecontrol",
    url = "https://github.com/PowerDNS/weakforced",
    packages = find_packages(),
    install_requires=install_reqs,
    include_package_data = True,
    scripts=['runreport.py'],
    long_description="The Report API is provided to enable access to the report information stored in Elasticsearch. It provides REST API endpoints to retrieve data about logins and devices, as well as endpoints to 'forget' devices and logins.",
    classifiers=[],
)

