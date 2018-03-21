[Contributor Covenant]: https://www.contributor-covenant.org
[Contributor Covenant's Code of Conduct]: https://www.contributor-covenant.org/version/1/4/code-of-conduct.html
[Code of Conduct]: /development/#code-of-conduct
[GitHub]: https://github.com/Kronuz/Xapiand
[GitHub Issue]: https://github.com/Kronuz/Xapiand/issues
[https://github.com/Kronuz/xapian]: https://github.com/Kronuz/xapian

# Contributing

This is an introduction to contributing to the Xapiand project.

---

The Xapiand project welcomes, and depends, on contributions from developers and
users in the open source community. Contributions can be made in a number of
ways, a few examples are:

- Code patches via pull requests
- Documentation improvements
- Bug reports and patch reviews

Everyone interacting in the Xapiand project's development, be it codebases, issue
trackers, chat rooms, and mailing lists is expected to follow the [Code of Conduct].

---

# Reporting an Issue

Please include as much detail as you can in the [GitHub Issue]. Let us know your
platform and Xapiand version. If you get an error please include the full error
and traceback when available.

# Submitting Pull Requests

Once you are happy with your changes or you are ready for some feedback, push
it to your fork and send a pull request. For a change to be accepted it will
most likely need to have tests and documentation if it is a new feature.

---

# Building from Source

First you'll need to fork and clone the repository from [GitHub]. Once you have a
local copy, procede with the build process.

## Requirements

Xapiand is written in C++14, it makes use of libev (which is included in the
codebase). The only external dependencies for building it are:

* Clang or GCC
* pkg-config
* CMake
* libpthread (internally used by the Standard C++ thread library)
* xapian-core v1.4+ (With patches by Kronuz applied, see [https://github.com/Kronuz/xapian])
* Optionally, Google's V8 Javascript engine library (tested with v5.1)

## Project Layout

	CMakeLists.txt   # The CMake configuration file.
	mkdocs.yml       # The documentation configuration file.
	docs/
		index.md     # The documentation homepage.
		...          # Other markdown pages, images and other files.
	src/

## Building process

1. Download and untar the Xapiand official distribution or clone repository
   from [GitHub].

2. Prepare build using:

```sh
	mkdir build
	cd build
	cmake -GNinja ..
```

3. build and install using:

```sh
	ninja
	ninja install
```

4. Run `xapiand` inside a new directory to be assigned to the node.

5. Run `curl 'http://localhost:8880/'`.


### Notes

* When preparing build for developing and debugging, generally you'd want to
  enable the Address Sanitizer, tracebacks in exceptions and debugging symbols:
  `cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DASAN=ON -DTRACEBACKS=ON ..`

* When compiling using ninja, the whole machine could slow down while compiling,
  since it uses all cores; you can prevent this by telling ninja to use
  `<number of cores> - 1` jobs. Example, for a system with 4 cores: `ninja -j3`.


#### macOS specifics


1. Simply installing Xcode will not install all of the command line developer
   tools, you must execute `xcode-select --install` in Terminal before trying
   to build.

2. You need cmake installed `brew install cmake`.

3. You need to request cmake to leave framework libraries last during the
   prepare build step above: `cmake -GNinja ..`


## Running the tests

```sh
	cmake check
```


---


# The Road Ahead

This is a list of features that are only partially implemented; but that are
planned to be supported by Xapiand some time soonish in order to get closer
to the final product:

* Multi-Partitioning and Distribution Strategies:
	* Social-Based Partitioning and Replication (SPAR <sup>[2](#footnote-2)</sup>).
	* Random Consistent Partitioning and Replication.

* Highly Available:
	* Automatic node operation rerouting.
	* Replicas exists to maximize high availability *and* data locality.
	* Read and Search operations performed on any of the replicas.
	* Reliable, asynchronous replication for long term persistency.


## Multi-Partitioning and Distribution Strategies

To achieve high availability, distribution of data and data locality, Xapiand
can partition, replicate and distribute indexes across several nodes using
any of the following partitioning strategies:


### Social-Based Partitioning and Replication

* Horizontal scaling by distributing indexes among several nodes.
* Maximizes data locality ensuring related indexes are kept (or are directly
  available) in the same node.
* Minimizes network usage when accessing a set of related indexes.


### Random Consistent Partitioning

* Horizontal scaling by distributing indexes among several nodes.


---


# Code of Conduct

## Our Pledge

In the interest of fostering an open and welcoming environment, we as
contributors and maintainers pledge to making participation in our project and
our community a harassment-free experience for everyone, regardless of age, body
size, disability, ethnicity, gender identity and expression, level of experience,
education, socio-economic status, nationality, personal appearance, race,
religion, or sexual identity and orientation.

## Our Standards

Examples of behavior that contributes to creating a positive environment
include:

* Using welcoming and inclusive language
* Being respectful of differing viewpoints and experiences
* Gracefully accepting constructive criticism
* Focusing on what is best for the community
* Showing empathy towards other community members

Examples of unacceptable behavior by participants include:

* The use of sexualized language or imagery and unwelcome sexual attention or
  advances
* Trolling, insulting/derogatory comments, and personal or political attacks
* Public or private harassment
* Publishing others' private information, such as a physical or electronic
  address, without explicit permission
* Other conduct which could reasonably be considered inappropriate in a
  professional setting

## Our Responsibilities

Project maintainers are responsible for clarifying the standards of acceptable
behavior and are expected to take appropriate and fair corrective action in
response to any instances of unacceptable behavior.

Project maintainers have the right and responsibility to remove, edit, or
reject comments, commits, code, wiki edits, issues, and other contributions
that are not aligned to this Code of Conduct, or to ban temporarily or
permanently any contributor for other behaviors that they deem inappropriate,
threatening, offensive, or harmful.

## Scope

This Code of Conduct applies both within project spaces and in public spaces
when an individual is representing the project or its community. Examples of
representing a project or community include using an official project e-mail
address, posting via an official social media account, or acting as an appointed
representative at an online or offline event. Representation of a project may be
further defined and clarified by project maintainers.

## Enforcement

Instances of abusive, harassing, or otherwise unacceptable behavior may be
reported by contacting the project team at [INSERT EMAIL ADDRESS]. All
complaints will be reviewed and investigated and will result in a response that
is deemed necessary and appropriate to the circumstances. The project team is
obligated to maintain confidentiality with regard to the reporter of an incident.
Further details of specific enforcement policies may be posted separately.

Project maintainers who do not follow or enforce the Code of Conduct in good
faith may face temporary or permanent repercussions as determined by other
members of the project's leadership.

## Attribution

This Code of Conduct is adapted from the [Contributor Covenant], version 1.4,
available at [Contributor Covenant's Code of Conduct]

---

# Maintenance Team

* <a href="https://twitter.com/germbravo">Germán Méndez Bravo (Kronuz)</a>
* José Madrigal Cárdenas (YosefMac)
* José María Valencia Ramírez (JoseMariaVR)
