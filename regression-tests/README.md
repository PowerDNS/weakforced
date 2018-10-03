README for Weakforced Regression Tests
------

Regression Tests are now designed to be run inside the regression docker container. This has necessitated some changes in the server names and port numbers.

Attempting to run the regression tests manually with ./runtests will fail. The correct way to run the regression tests is with "make regression" in the docker directory, which will also run various other tests including distchecks.
