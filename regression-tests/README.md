README for Weakforced Regression Tests
------

Regression Tests are now designed to be run inside the regression docker container. This has necessitated some changes in the server names and port numbers.

Attempting to run the regression tests manually with ./runtests will fail. The correct way to run the regression tests is with "make regression" in the docker directory, which will also run various other tests including distchecks.

Running a specific test
------

Run the following in docker directory to start the test environment:

```
make build_image start
```

Start bash in regression environment:

```
docker-compose exec regression bash
```

In the bash session, run:

```
autoreconf -v -i -f
./configure --enable-trackalert --enable-systemd --disable-docker \
  --enable-unit-tests --enable-asan --enable-ubsan \
  --disable-silent-rules CC=clang CXX=clang++
make -j
```

Now you can run only `test_Basics.py` with:

```
cd regression-tests
./runtests test_Basics.py
```
