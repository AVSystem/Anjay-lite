<a id="readme-top"></a>
# Anjay Lite LwM2M Client SDK [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">][avsystem-url]

<!-- PROJECT BADGES -->
<!--[![Build Status](https://github.com/AVSystem/Anjay-lite/actions/workflows/anjay-lite-tests.yml/badge.svg?branch=master)](https://github.com/AVSystem/Anjay-lite/actions) -->
[![License][dual-license-badge]](LICENSE)

## Licensing Notice

### Mandatory Registration for Commercial Use

If you intend to use Anjay Lite in any **commercial context**,
**you must fill in a registration form** to obtain a **free commercial license**
for your product.

**Register** [**here**][anjay-lite-registration].

**Why is registration required?**

We introduced registration to:

- **Gain insight into usage patterns** – so we can prioritize support, features,
  and enhancements relevant to real-world use cases.
- **Engage with users** – allow us to notify you about important updates,
  security advisories, or licensing changes.
- **Offer tailored commercial plugins, professional services, and technical support**
  to accelerate your product development.

For inquiries, please contact: [sales@avsystem.com](mailto:sales@avsystem.com)

## Beta Release Notice

This is a beta release of Anjay Lite and is currently under active development.
While we are making every effort to keep the API stable, changes may still occur
as we refine the library based on testing and user input.

We encourage you to explore the SDK and share your feedback, suggestions, or
issues via our GitHub repository.

## Table of Contents

* [About The Project](#about-the-project)
* [About OMA LwM2M](#about-oma-lwm2m)
* [Quickstart Guide](#quickstart-guide)
  * [Building and Running a Single Anjay Lite Example](#building-and-running-a-single-anjay-lite-example)
  * [Configuring the MbedTLS](#configuring-the-mbedtls)
* [Documentation](#documentation)
* [License](#license)
* [Commercial Support](#commercial-support)
  * [LwM2M Server](#lwm2m-server)
* [Contact](#contact)
* [Contributing](#contributing)

## About The Project

Anjay Lite is a streamlined version of our robust [Anjay LwM2M Client SDK][anjay-github],
purpose-built for the most resource-constrained and battery-powered IoT devices.
Designed with ultra-efficiency in mind, Anjay Lite eliminates many abstractions
and embraces a minimalistic architecture that significantly reduces memory and
code footprint. It is purpose-built for highly resource-constrained environments,
including bare-metal devices that operate without an operating system or dynamic
memory allocation.

By offering developers direct, fine-grained control over resource usage and client
behavior, Anjay Lite empowers precision tailoring of LwM2M functionality to the
specific constraints and requirements of embedded applications — ideal for sectors
such as smart water metering, asset tracking, and environmental monitoring.

While Anjay remains the go-to solution for feature-rich, scalable LwM2M
implementations — supporting a broad range of use cases and advanced
capabilities, Anjay Lite addresses a complementary need: delivering lightweight
LwM2M connectivity without compromise on reliability or standards compliance.

The project has been created and is actively maintained by
[AVSystem][avsystem-url].

For more information and a list of supported features, see the
[Anjay Lite Introduction][anjay-lite-introduction].

<p align="right">(<a href="#readme-top">Back to top</a>)</p>

## About OMA LwM2M

OMA LwM2M is a remote device management and telemetry protocol designed to
conserve network resources. It is especially suitable for constrained wireless
devices, where network communication is a major factor affecting battery life.
LwM2M features secure (DTLS-encrypted) methods of remote bootstrapping,
configuration and notifications over UDP or SMS.

More details about OMA LwM2M: [Brief introduction to LwM2M][lwm2m-introduction]

<p align="right">(<a href="#readme-top">Back to top</a>)</p>

## Quickstart Guide

To build the Anjay Lite project, run:

``` sh
mkdir build
cd build
cmake ..
make -j
```

This will compile all the examples that use Anjay Lite, along with the test suite
in the build directory.

### Building and Running a Single Anjay Lite Example

To build and run a specific Anjay Lite example (e.g., from the examples/tutorial/BC-MandatoryObjects
directory), you can follow these steps:

``` sh
cd examples/tutorial/BC-MandatoryObjects
mkdir build
cd build
cmake ..
make -j
./anjay_lite_bc_mandatory_objects <endpoint_name>
```

Replace <endpoint_name> with your desired endpoint name.

### Configuring the MbedTLS

Anjay Lite uses Mbed TLS to implement DTLS for secure transport.
The library automatically fetches and builds MbedTLS version 3.6.0 during the
build process. You can override this default version or use a custom, pre-installed MbedTLS library if needed.

To override the default version fetched via FetchContent, set the MBEDTLS_VERSION variable:

``` sh
cd examples/tutorial/mbedtls-build
mkdir build
cd build
cmake .. -DMBEDTLS_VERSION=3.5.0
make -j
```

If you want to use your own prebuilt version of MbedTLS with custom configuration, follow these steps:

1. Build and install MbedTLS manually (in specified directory, without affecting global installed MbedTLS):

``` sh
git clone https://github.com/ARMmbed/mbedtls.git
cd mbedtls
git checkout v3.4.0 # or another supported version
# change configuration using scripts/config.py or modify mbedtls_config.h directly
cmake . -DCMAKE_INSTALL_PREFIX=$PWD/install
make install
```

2. Build Anjay Lite with that installed version:

``` sh
cd /path/to/anjay-lite/build
cmake .. -DMBEDTLS_ROOT_DIR=mbedtls-path/mbedtls/install
make -j
```

Anjay Lite requires MbedTLS 3.x. Versions from the 2.x series are not supported.

<p align="right">(<a href="#readme-top">Back to top</a>)</p>

## Documentation

To get started with Anjay Lite, refer to our documentation:

- [Compilation instructions][anjay-lite-compilation]
- [Full documentation][anjay-lite-full-documentation]
- [Tutorials][anjay-lite-tutorials]
- [API docs][anjay-lite-api-docs]

<p align="right">(<a href="#readme-top">Back to top</a>)</p>


<!-- LICENSE && COMMERCIAL SUPPORT-->
## License

This project is available under a dual-licensing model:

- A free license for non-commercial use, including evaluation, academic research, and hobbyist projects,
- A commercial license for use in proprietary products and commercial deployments.

See [LICENSE](LICENSE) for terms and conditions.

<p align="right">(<a href="#readme-top">Back to top</a>)</p>

## Commercial Support

Anjay Lite LwM2M Client SDK comes with the option of [full commercial support, provided by AVSystem][avsystem-anjay-lite-url].

### LwM2M Server
If you're interested in LwM2M Server, be sure to check out the [Coiote IoT Device Management][avsystem-coiote-url]
platform by AVSystem. It also includes the [interoperability test module][avsystem-coiote-interoperability-test-url]
that you can use to test your LwM2M client implementation. Our automated tests
and testing scenarios enable you to quickly check how interoperable your device
is with LwM2M.

<p align="right">(<a href="#readme-top">Back to top</a>)</p>

## Contact

Find us on [Discord][avsystem-discord] or contact us [directly][avsystem-contact].

<p align="right">(<a href="#readme-top">Back to top</a>)</p>

## Contributing

Contributions are welcome! See our [contributing guide](CONTRIBUTING.rst).

<p align="right">(<a href="#readme-top">Back to top</a>)</p>

<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[avsystem-url]: https://avsystem.com
[avsystem-anjay-lite-url]: https://go.avsystem.com/anjay-lite
[avsystem-coiote-url]: https://www.avsystem.com/products/coiote-iot-dm
[avsystem-coiote-interoperability-test-url]: https://avsystem.com/coiote-iot-device-management-platform/lwm2m-interoperability-test
[lwm2m-introduction]: https://avsystem.com/crashcourse/lwm2m
[anjay-github]: https://github.com/AVSystem/Anjay
[avsystem-contact]: https://avsystem.com/contact
[avsystem-discord]: https://discord.com/invite/UxSxbZnU98

<!-- Badges -->
[dual-license-badge]: https://img.shields.io/badge/license-Dual-blue

[anjay-lite-full-documentation]: https://AVSystem.github.io/Anjay-lite
[anjay-lite-introduction]: https://AVSystem.github.io/Anjay-lite/Introduction.html
[anjay-lite-compilation]: https://AVSystem.github.io/Anjay-lite/Compiling_client_applications.html
[anjay-lite-tutorials]: https://AVSystem.github.io/Anjay-lite/BasicClient.html
[anjay-lite-api-docs]: https://AVSystem.github.io/Anjay-lite/api
[anjay-lite-registration]: https://go.avsystem.com/anjay-lite-registration
