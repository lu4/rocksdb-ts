nbind-example-minimal
=====================

[![build status](https://travis-ci.org/charto/nbind-example-minimal.svg?branch=master)](http://travis-ci.org/charto/nbind-example-minimal)

This is an example Node.js addon written in C++ that prints "Hello, &lt;name&gt;!"

It demonstrates using [nbind](https://github.com/charto/nbind)
to easily define an API for the addon and [autogypi](https://github.com/charto/autogypi)
to prepare the project for compiling.

You need one of the following C++ compilers to install it:

- GCC 4.8 or above
- Clang 3.6 or above
- Visual Studio 2015 ([The Community version](https://www.visualstudio.com/en-us/products/visual-studio-community-vs.aspx) is fine)

Usage
-----

```bash
git clone https://github.com/charto/nbind-example-minimal.git
cd nbind-example-minimal
npm install && npm test
```

License
=======

This example is public domain.
