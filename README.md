<a id="readme-top"></a>
# Anjay Lite LwM2M Client SDK [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">][avsystem-url]

<!-- PROJECT BADGES -->
<!--[![Build Status](https://github.com/AVSystem/Anjay-lite/actions/workflows/anjay-lite-tests.yml/badge.svg?branch=master)](https://github.com/AVSystem/Anjay-lite/actions)-->
[![License][dual-license-badge]](LICENSE)

---

## Licensing Notice

### Mandatory Registration for Commercial Use

If you intend to use Anjay Lite in any **commercial context**,
**you must fill in a registration form** to obtain a **free commercial license**
for your product.

**Register** [**here**][anjay-lite-registration].

**Why is registration required?**

We introduced registration to:

- **Gain insight into usage patterns** — so we can prioritize support, features,
  and enhancements relevant to real-world use cases.
- **Engage with users** — allow us to notify you about important updates,
  security advisories, or licensing changes.
- **Offer tailored commercial plugins, professional services, and technical support**
  to accelerate your product development.

For inquiries, please contact: [sales@avsystem.com](mailto:sales@avsystem.com)

---

## Table of Contents

* [About The Project](#about-the-project)
* [Quickstart Guide](#quickstart-guide)
* [Documentation](#documentation)
* [License](#license)
* [Commercial Support](#commercial-support)
* [LwM2M Server](#lwm2m-server)
* [Contact](#contact)
* [Feedback](#feedback)
* [Contributing](#contributing)

## About The Project

Anjay Lite is a lightweight version of the [Anjay LwM2M Client SDK][anjay-github],
designed specifically for highly resource-constrained, battery-powered IoT devices.
It features a minimalistic architecture that reduces memory and code footprint,
offering fine-grained control over resource usage and client behavior.

Anjay Lite is ideal for bare-metal devices without operating systems or dynamic
memory allocation. It is optimized for embedded applications such as:
- Smart water metering
- Asset tracking
- Environmental monitoring

While Anjay remains the go-to solution for feature-rich, scalable LwM2M
implementations — supporting a broad range of use cases and advanced
capabilities, Anjay Lite addresses a complementary need: delivering lightweight
LwM2M connectivity without compromise on reliability or standards compliance.

The project is actively developed and maintained by
[AVSystem][avsystem-url].

For more information and a list of supported features, see the
[Anjay Lite Introduction][anjay-lite-introduction].

<p align="right">(<a href="#readme-top">Back to top</a>)</p>

**OMA LwM2M**

OMA Lightweight M2M is a communication protocol for remote device management and 
elemetry. It is designed for constrained wireless devices, where communication
efficiency impacts battery life. LwM2M supports secure bootstrapping, configuration,
and notifications over UDP or SMS using DTLS encryption.

More details about OMA LwM2M: [Brief introduction to LwM2M][lwm2m-introduction]

<p align="right">(<a href="#readme-top">Back to top</a>)</p>

---

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

### Building the documentation

To build the documentation, first install all dependencies by following the `Prepare the environment`
section in [Compilation instructions][anjay-lite-compilation]. Then run:

``` sh
mkdir build
cd build
cmake ..
make doc
```

The generated documentation will be available in the `doc/build` directory.

### Configuring the MbedTLS

Anjay Lite examples uses MbedTLS to implement DTLS for secure transport.
The library automatically fetches and builds MbedTLS version 3.6.4 during the
build process. You can override this default version or use a custom, pre-installed MbedTLS library if needed.

To override the default version fetched via FetchContent, set the MBEDTLS_VERSION variable:

``` sh
cd examples/tutorial/BC-Security
mkdir build
cd build
cmake .. -DMBEDTLS_VERSION=3.5.0
make -j
```

To use your own custom-built MbedTLS (for example, with specific configuration settings):

1. Clone and build MbedTLS locally:

``` sh
git clone https://github.com/ARMmbed/mbedtls.git
cd mbedtls
git checkout v3.4.0 # or another supported version
# change configuration using scripts/config.py or modify mbedtls_config.h directly
cmake . -DCMAKE_INSTALL_PREFIX=$PWD/install
make install
```

2. Build Anjay Lite using your installed version:

``` sh
cd /path/to/anjay-lite/build
cmake .. -DMBEDTLS_ROOT_DIR=mbedtls-path/mbedtls/install
make -j
```

Make sure to replace `/path/to/mbedtls/install` with the actual path to your
installation.

> Anjay Lite requires MbedTLS 3.x. Versions from the 2.x series are not supported.

<p align="right">(<a href="#readme-top">Back to top</a>)</p>

## Documentation

Visit the official Anjay Lite documentation:

- [Compilation instructions][anjay-lite-compilation]
- [Full documentation][anjay-lite-full-documentation]
- [Tutorials][anjay-lite-tutorials]
- [API docs][anjay-lite-api-docs]

<p align="right">(<a href="#readme-top">Back to top</a>)</p>

---

## Embedded Platform Integrations

Anjay Lite is designed with portability in mind and can be integrated into
various embedded environments. For details, see the
[integrations chapter in the documentation][anjay-lite-integrations].

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

## LwM2M Server
If you're interested in LwM2M Server, be sure to check out the [Coiote IoT Device Management][avsystem-coiote-url]
platform by AVSystem. It also includes the [interoperability test module][avsystem-coiote-interoperability-test-url]
that you can use to test your LwM2M client implementation. Our automated tests
and testing scenarios enable you to quickly check how interoperable your device
is with LwM2M.

<p align="right">(<a href="#readme-top">Back to top</a>)</p>

## Contact

Find us on [Discord][avsystem-discord] or contact us [directly][avsystem-contact].

<p align="right">(<a href="#readme-top">Back to top</a>)</p>

## Feedback

Got a minute? Help improve **Anjay IoT SDK - [fill out this short form][anjay-feedback]**.

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
[anjay-feedback]: https://docs.google.com/forms/d/e/1FAIpQLSegs_HTDEM-J3w0VeEvVdVTsjiB41YKxj_4w9dud0GQsUIQiA/viewform

<!-- Badges -->
[dual-license-badge]: https://img.shields.io/badge/license-Dual-blue

[anjay-lite-full-documentation]: https://AVSystem.github.io/Anjay-lite
[anjay-lite-integrations]: https://avsystem.github.io/Anjay-lite/Integrations.html
[anjay-lite-introduction]: https://AVSystem.github.io/Anjay-lite/Introduction.html
[anjay-lite-compilation]: https://avsystem.github.io/Anjay-lite/Compile_client_applications.html
[anjay-lite-tutorials]: https://AVSystem.github.io/Anjay-lite/BasicClient.html
[anjay-lite-api-docs]: https://AVSystem.github.io/Anjay-lite/api
[anjay-lite-registration]: https://go.avsystem.com/anjay-lite-registration
