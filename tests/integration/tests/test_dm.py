# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

from framework_tools.lwm2m.server import Lwm2mServer, coap
import framework_tools.lwm2m.messages as msgs
from framework_tools.lwm2m.coap.packet import ANY
from framework_tools.lwm2m.coap.transport import Transport
from framework_tools.lwm2m.senml_cbor import CBOR, SenmlLabel
from framework_tools.lwm2m.tlv import TLV

import cbor2
import json
import utils

ENDPOINT = "test-endpoint"
LIFETIME = 100


def _make_nosec_server():
    return Lwm2mServer(coap.Server(transport=Transport.UDP))


def _server_uri(server):
    return f"coap://127.0.0.1:{server.get_listen_port()}"


def _init_app_nosec_server(app,
                           server,
                           endpoint=ENDPOINT,
                           lifetime=LIFETIME):
    config = {
        "endpoint": endpoint,
        "servers": [{
            "uri": _server_uri(server),
            "security": {
                "kind": "nosec"
            },
            "bootstrap": False,
            "lifetime": lifetime,
        }],
    }

    assert app.rpc.call("init", config) == 0


def _handle_register(server,
                     endpoint=ENDPOINT,
                     lifetime=LIFETIME,
                     timeout_s=None):
    expected = msgs.Lwm2mRegister(
        f"/rd?ep={endpoint}&lt={lifetime}&lwm2m=1.2&b=U")
    pkt = server.recv(
        timeout_s=timeout_s) if timeout_s is not None else server.recv()
    utils.assert_msg_equal(expected, pkt)
    assert pkt.content is not None
    server.send(
        msgs.Lwm2mCreated.matching(pkt)(
            location=f"/rd/{endpoint}"))


def _read(server, path, format=None, content=None, error_code=None, options=ANY):
    read = msgs.Lwm2mRead(path, accept=format, options=options)
    server.send(read)
    if error_code is None:
        utils.assert_msg_equal(
            msgs.Lwm2mContent.matching(read)(content=content),
            server.recv())
    else:
        utils.assert_msg_equal(
            msgs.Lwm2mErrorResponse.matching(read)(code=error_code),
            server.recv())


def _write(server, path, format, content, update=False, error_code=None):
    write = msgs.Lwm2mWrite(path, format=format,
                            content=content, update=update)
    server.send(write)
    if error_code is None:
        utils.assert_msg_equal(
            msgs.Lwm2mChanged.matching(write)(), server.recv())
    else:
        utils.assert_msg_equal(
            msgs.Lwm2mErrorResponse.matching(write)(code=error_code),
            server.recv())


def _write_and_read(server, path, format, content, update=False):
    _write(server, path, format, content, update)

    _read(server, path, format, content)


def test_write_and_read_on_different_path_deep(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server)
        _handle_register(server)

        # resource instance
        _write_and_read(server, "/1234/0/2/0",
                        format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
                        content=CBOR.serialize([{SenmlLabel.BASE_NAME: "/1234/0/2/0", SenmlLabel.VALUE: 2}]))

        # resource
        _write_and_read(server, "/1/0/5",
                        format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
                        content=CBOR.serialize([{SenmlLabel.BASE_NAME: "/1/0/5", SenmlLabel.VALUE: 2}]))

        # instance
        _write(server, "/1234/0",
               format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
               content=CBOR.serialize([{SenmlLabel.BASE_NAME: "/1234/0", SenmlLabel.NAME: "/2/0", SenmlLabel.VALUE: 2},
                                       {SenmlLabel.NAME: "/3", SenmlLabel.VALUE: 2.1}]))

        _read(server, "/1234/0",
              format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
              content=CBOR.serialize([{SenmlLabel.BASE_NAME: "/1234/0", SenmlLabel.NAME: "/0", SenmlLabel.VALUE: 0.0},
                                      {SenmlLabel.NAME: "/1",
                                       SenmlLabel.STRING: "first"},
                                      {SenmlLabel.NAME: "/2/0", SenmlLabel.VALUE: 2}]))

        _write(server, "/1234/0",
               format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
               content=CBOR.serialize([{SenmlLabel.BASE_NAME: "/1234/0/", SenmlLabel.NAME: "2/0", SenmlLabel.VALUE: 2},
                                       {SenmlLabel.NAME: "3",
                                           SenmlLabel.VALUE: 2.1},
                                       {SenmlLabel.NAME: "8888",
                                        SenmlLabel.STRING: "hehe"},
                                       ]))

        # TODO anjay behavior depends on senml type
        _write(server, "/1234/0",
               format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
               content=CBOR.serialize([{SenmlLabel.BASE_NAME: "/1234/0/", SenmlLabel.NAME: "2/0", SenmlLabel.VALUE: 2},
                                       {SenmlLabel.NAME: "3",
                                           SenmlLabel.VALUE: 2.1},
                                       {SenmlLabel.NAME: "8888",
                                           SenmlLabel.VALUE: 2.1},
                                       ]), error_code=coap.Code.RES_NOT_FOUND)

        # object

        # write operation cannot be performe on Object

        _read(server, '/1234',
              format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
              content=CBOR.serialize([
                  {SenmlLabel.BASE_NAME: "/1234",
                      SenmlLabel.NAME: "/0/0", SenmlLabel.VALUE: 0.0},
                  {SenmlLabel.NAME: "/0/1", SenmlLabel.STRING: "first"},
                  {SenmlLabel.NAME: "/0/2/0", SenmlLabel.VALUE: 2},
                  {SenmlLabel.NAME: "/1/0", SenmlLabel.VALUE: 1.0},
                  {SenmlLabel.NAME: "/1/1", SenmlLabel.STRING: "second"},
                  {SenmlLabel.NAME: "/1/2/0", SenmlLabel.VALUE: 0},
                  {SenmlLabel.NAME: "/2/0", SenmlLabel.VALUE: 2.0},
                  {SenmlLabel.NAME: "/2/1", SenmlLabel.STRING: "third"},
                  {SenmlLabel.NAME: "/2/2/0", SenmlLabel.VALUE: 0}
              ]))


def test_read_all_existing_content_formats(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server)
        _handle_register(server)

        _read(server, "/1/0/5",
              options=[coap.Option.ACCEPT(coap.ContentFormat.TEXT_PLAIN),
                       coap.Option.ACCEPT(coap.ContentFormat.APPLICATION_LINK),
                       coap.Option.ACCEPT(
                           coap.ContentFormat.APPLICATION_OCTET_STREAM),
                       coap.Option.ACCEPT(
                           coap.ContentFormat.APPLICATION_LWM2M_TLV),
                       coap.Option.ACCEPT(
                           coap.ContentFormat.APPLICATION_LWM2M_JSON),
                       coap.Option.ACCEPT(coap.ContentFormat.APPLICATION_CBOR),
                       coap.Option.ACCEPT(
                           coap.ContentFormat.APPLICATION_LWM2M_SENML_JSON),
                       coap.Option.ACCEPT(
                           coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR),
                       coap.Option.ACCEPT(
                           coap.ContentFormat.APPLICATION_LWM2M_CBOR),
                       coap.Option.ACCEPT(
                           coap.ContentFormat.APPLICATION_LWM2M_SENML_ETCH_JSON),
                       coap.Option.ACCEPT(coap.ContentFormat.APPLICATION_LWM2M_SENML_ETCH_CBOR)],
              content=b'86400')


def test_write_and_read_resource_different_content_formats(app_spawner):
    PATH_TO_DISABLE_TIMEOUT = "/1/0/5"
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server)
        _handle_register(server)

        _write_and_read(server, PATH_TO_DISABLE_TIMEOUT,
                        format=coap.ContentFormat.TEXT_PLAIN, content=b'10')

        _write(server, PATH_TO_DISABLE_TIMEOUT,
               format=coap.ContentFormat.APPLICATION_LWM2M_TLV, content=TLV.make_resource(5, 20).serialize())

        # TLV can be decoded, but Anjay cannot encode its own messages in this content format
        _read(server, PATH_TO_DISABLE_TIMEOUT,
              format=coap.ContentFormat.TEXT_PLAIN, content=b'20')

        _write_and_read(server, PATH_TO_DISABLE_TIMEOUT,
                        format=coap.ContentFormat.APPLICATION_CBOR,
                        content=cbor2.dumps(30))

        _write_and_read(server, PATH_TO_DISABLE_TIMEOUT,
                        format=coap.ContentFormat.APPLICATION_LWM2M_CBOR,
                        content=cbor2.dumps({1: {0: {5: 40}}}, indefinite_containers=True))

        _write_and_read(server, PATH_TO_DISABLE_TIMEOUT,
                        format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
                        content=CBOR.serialize([{SenmlLabel.BASE_NAME: "/1/0/5", SenmlLabel.VALUE: 50}]))

        _write_and_read(server, PATH_TO_DISABLE_TIMEOUT,
                        format=coap.ContentFormat.APPLICATION_LWM2M_SENML_ETCH_CBOR,
                        content=CBOR.serialize([{SenmlLabel.BASE_NAME: "/1/0/5", SenmlLabel.VALUE: 60}]))


def test_write_and_read_resource_unsupported_content_format(app_spawner):
    PATH_TO_DISABLE_TIMEOUT = "/1/0/5"
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server)
        _handle_register(server)

        _write(server, PATH_TO_DISABLE_TIMEOUT,
               format=coap.ContentFormat.APPLICATION_LWM2M_SENML_JSON,
               content=json.dumps({'n': '/1/0/5', 'v': 70}), error_code=coap.Code.RES_UNSUPPORTED_CONTENT_FORMAT)

        _read(server, PATH_TO_DISABLE_TIMEOUT,
              format=coap.ContentFormat.APPLICATION_LWM2M_TLV,
              error_code=coap.Code.RES_NOT_ACCEPTABLE)


def test_write_and_read_resource_unexisting_path(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server)
        _handle_register(server)

        _write(server, '/1/0/56',
               format=coap.ContentFormat.TEXT_PLAIN,
               content=b'80', error_code=coap.Code.RES_NOT_FOUND),

        _read(server, '/1/0/56',
              format=coap.ContentFormat.APPLICATION_LWM2M_TLV,
              error_code=coap.Code.RES_NOT_FOUND)


def test_read_write_on_unsupported_resources(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server)
        _handle_register(server)

        _write(server, '/1234/0/0',
               format=coap.ContentFormat.TEXT_PLAIN,
               content=b'2.1', error_code=coap.Code.RES_METHOD_NOT_ALLOWED),

        _read(server, '/1234/0/3',
              format=coap.ContentFormat.APPLICATION_LWM2M_TLV,
              error_code=coap.Code.RES_METHOD_NOT_ALLOWED)


def test_write_resource_instances(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server)
        _handle_register(server)

        # write to resource instance
        _write_and_read(server, '/1234/0/2/0',
                        format=coap.ContentFormat.TEXT_PLAIN, content=b'7')

        # write to resource instance with create (without removing already existing resource instances)
        _write(server, '/1234/0',
               format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
               content=CBOR.serialize([
                   {SenmlLabel.BASE_NAME: "/1234/0/2/2137", SenmlLabel.VALUE: 12},
               ]), update=True)

        # check updated content
        _read(server, '/1234/0/2',
              format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
              content=CBOR.serialize([
                  {SenmlLabel.BASE_NAME: "/1234/0/2",
                   SenmlLabel.NAME: "/0", SenmlLabel.VALUE: 7},
                  {SenmlLabel.NAME: "/2137", SenmlLabel.VALUE: 12},
              ]))

        # write to resource instance with create (with removing already existing resource instances)
        _write(server, '/1234/0',
               format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
               content=CBOR.serialize([
                   {SenmlLabel.NAME: "/1234/0/2/6666", SenmlLabel.VALUE: 38},
               ]))

        # check updated content
        _read(server, '/1234/0/2',
              format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
              content=CBOR.serialize([
                  {SenmlLabel.BASE_NAME: "/1234/0/2",
                   SenmlLabel.NAME: "/6666", SenmlLabel.VALUE: 38}
              ]))

        # write too many resurce instances
        _write(server, '/1234/0',
        format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
        content=CBOR.serialize([
            {SenmlLabel.NAME: "/1234/0/2/6666", SenmlLabel.VALUE: 38},
            {SenmlLabel.NAME: "/1234/0/2/6767", SenmlLabel.VALUE: 39},
            {SenmlLabel.NAME: "/1234/0/2/6969", SenmlLabel.VALUE: 40},
        ]), error_code=coap.Code.RES_INTERNAL_SERVER_ERROR)

        # check that other instances are untouched
        _read(server, '/1234',
              format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
              content=CBOR.serialize([
                  {SenmlLabel.BASE_NAME: "/1234",
                      SenmlLabel.NAME: "/0/0", SenmlLabel.VALUE: 0.0},
                  {SenmlLabel.NAME: "/0/1", SenmlLabel.STRING: "first"},
                  {SenmlLabel.NAME: "/0/2/6666", SenmlLabel.VALUE: 38},
                  {SenmlLabel.NAME: "/1/0", SenmlLabel.VALUE: 1.0},
                  {SenmlLabel.NAME: "/1/1", SenmlLabel.STRING: "second"},
                  {SenmlLabel.NAME: "/1/2/0", SenmlLabel.VALUE: 0},
                  {SenmlLabel.NAME: "/2/0", SenmlLabel.VALUE: 2.0},
                  {SenmlLabel.NAME: "/2/1", SenmlLabel.STRING: "third"},
                  {SenmlLabel.NAME: "/2/2/0", SenmlLabel.VALUE: 0}
              ]))


def _read_composite(server, paths, format, content=None, error_code=None):
    read_composite = msgs.Lwm2mReadComposite(paths, accept=format)
    server.send(read_composite)
    if error_code is None:
        utils.assert_msg_equal(
            msgs.Lwm2mContent.matching(read_composite)(content=content),
            server.recv())
    else:
        utils.assert_msg_equal(
            msgs.Lwm2mErrorResponse.matching(read_composite)(code=error_code),
            server.recv())


def _write_composite(server, format, content, error_code=None):
    write_composite = msgs.Lwm2mWriteComposite(format=format, content=content)
    server.send(write_composite)
    if error_code is None:
        utils.assert_msg_equal(
            msgs.Lwm2mChanged.matching(write_composite)(), server.recv())
    else:
        utils.assert_msg_equal(
            msgs.Lwm2mErrorResponse.matching(write_composite)(code=error_code),
            server.recv())


def test_read_composite(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server)
        _handle_register(server)

        # read from two different resources
        _read_composite(server, ['/1/0/1', '/1234/0/0'],
                        format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
                        content=CBOR.serialize([
                            {SenmlLabel.NAME: "/1/0/1", SenmlLabel.VALUE: 100},
                            {SenmlLabel.NAME: "/1234/0/0", SenmlLabel.VALUE: 0.0}
                        ]))

        # read from existing and not existing resource
        _read_composite(server, ['/1/0/1', '/1244/23/34'],
                        format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
                        content=CBOR.serialize([
                            {SenmlLabel.NAME: "/1/0/1", SenmlLabel.VALUE: 100},
                        ]))

        # more records than ANJ_DM_MAX_COMP_READ_ENTRIES
        _read_composite(server, ['/1/0/1', '/1/0/2', '/1/0/3', '/1/0/5', '/1/0/6', "/1234/0/0"],
                        format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
                        error_code=coap.Code.RES_INTERNAL_SERVER_ERROR)


def test_write_composite(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server)
        _handle_register(server)

        # Write to two resources
        _write_composite(server,
                         format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
                         content=CBOR.serialize([
                             {SenmlLabel.NAME: "/1/0/5", SenmlLabel.VALUE: 7312},
                             {SenmlLabel.NAME: "/1234/0/2/0", SenmlLabel.VALUE: 200}
                         ]))

        # check updated content
        _read_composite(server, ['/1/0/5', '/1234/0/2/0'],
                        format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
                        content=CBOR.serialize([
                            {SenmlLabel.NAME: "/1/0/5", SenmlLabel.VALUE: 7312},
                            {SenmlLabel.NAME: "/1234/0/2/0", SenmlLabel.VALUE: 200}
                        ]))

        # Write to one existing and one non-existing resource
        _write_composite(server,
                         format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
                         content=CBOR.serialize([
                             {SenmlLabel.NAME: "/1/0/5", SenmlLabel.VALUE: 4567},
                             {SenmlLabel.NAME: "/3/0/15", SenmlLabel.VALUE: 200.0}
                         ]), error_code=coap.Code.RES_NOT_FOUND)
        # TODO anjay behavior depends on senml type
        _write_composite(server,
                         format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
                         content=CBOR.serialize([
                             {SenmlLabel.NAME: "/1/0/5", SenmlLabel.VALUE: 4567},
                             {SenmlLabel.NAME: "/3/0/15",
                                 SenmlLabel.STRING: "hehe"}
                         ]))


def _create(server, path, format=None, content=None, error_code=None):
    kwargs = {'path': path}
    if format is not None:
        kwargs['format'] = format
    if content is not None:
        kwargs['content'] = content
    create = msgs.Lwm2mCreate(**kwargs)
    server.send(create)
    if error_code is None:
        utils.assert_msg_equal(
            msgs.Lwm2mCreated.matching(create)(), server.recv())
    else:
        utils.assert_msg_equal(
            msgs.Lwm2mErrorResponse.matching(create)(code=error_code),
            server.recv())


def test_create(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server)
        _handle_register(server)

        # third object instance doesn't exist
        _read(server, '/1234/3',
              format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
              error_code=coap.Code.RES_NOT_FOUND)

        # create without payload
        _create(server, '/1234')

        # now third object instance exists
        _read(server, '/1234/3',
              format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
              content=CBOR.serialize([
                  {SenmlLabel.BASE_NAME: "/1234/3",
                      SenmlLabel.NAME: "/0", SenmlLabel.VALUE: 3.0},
                  {SenmlLabel.NAME: "/1", SenmlLabel.STRING: 'fourth'},
                  {SenmlLabel.NAME: "/2/0", SenmlLabel.VALUE: 0}
              ]))

        # fifth object instance doesn't exist
        _read(server, '/1234/5',
              format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
              error_code=coap.Code.RES_NOT_FOUND)

        # create with payload
        _create(server, '/1234',
                format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
                content=CBOR.serialize([
                    {SenmlLabel.BASE_NAME: "/1234/5",
                        SenmlLabel.NAME: "/2/0", SenmlLabel.VALUE: 100},
                    {SenmlLabel.NAME: "/2/1", SenmlLabel.VALUE: 200}
                ]))

        # now fifth object instance exists
        fifth_object_state = CBOR.serialize([
            {SenmlLabel.BASE_NAME: "/1234/5",
                SenmlLabel.NAME: "/0", SenmlLabel.VALUE: 5.0},
            {SenmlLabel.NAME: "/1", SenmlLabel.STRING: 'sixth'},
            {SenmlLabel.NAME: "/2/0", SenmlLabel.VALUE: 100},
            {SenmlLabel.NAME: "/2/1", SenmlLabel.VALUE: 200}
        ])
        _read(server, '/1234/5',
              format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
              content=fifth_object_state)

        # try to create already existing instance
        _create(server, '/1234',
                format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
                content=CBOR.serialize([
                    {SenmlLabel.BASE_NAME: "/1234/5",
                        SenmlLabel.NAME: "/2/0", SenmlLabel.VALUE: 500}
                ]), error_code=coap.Code.RES_BAD_REQUEST)

        # instance stay unchanged
        _read(server, '/1234/5',
              format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
              content=fifth_object_state)

        # create with payload, too many resource instances
        _create(server, '/1234',
                format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
                content=CBOR.serialize([
                    {SenmlLabel.BASE_NAME: "/1234/6",
                        SenmlLabel.NAME: "/2/0", SenmlLabel.VALUE: 100},
                    {SenmlLabel.NAME: "/2/1", SenmlLabel.VALUE: 200},
                    {SenmlLabel.NAME: "/2/2", SenmlLabel.VALUE: 300}
                ]), error_code=coap.Code.RES_INTERNAL_SERVER_ERROR)

        # create without payload, try to create too many instances
        _create(server, '/1234')
        _create(server, '/1234', error_code=coap.Code.RES_INTERNAL_SERVER_ERROR)


def _delete(server, path, error_code=None):
    delete = msgs.Lwm2mDelete(path)
    server.send(delete)
    if error_code is None:
        utils.assert_msg_equal(
            msgs.Lwm2mDeleted.matching(delete)(), server.recv())
    else:
        utils.assert_msg_equal(
            msgs.Lwm2mErrorResponse.matching(delete)(code=error_code),
            server.recv())


def test_delete(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server)
        _handle_register(server)

        # second object instance exists
        _read(server, '/1234/2',
              format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
              content=CBOR.serialize([
                  {SenmlLabel.BASE_NAME: "/1234/2",
                   SenmlLabel.NAME: "/0", SenmlLabel.VALUE: 2.0},
                  {SenmlLabel.NAME: "/1", SenmlLabel.STRING: 'third'},
                  {SenmlLabel.NAME: "/2/0", SenmlLabel.VALUE: 0},
              ]))

        # delete resource instance
        _delete(server, '/1234/2/2/0')

        # zeroth resource instance doesn't exist
        _read(server, '/1234/2/2/0',
              format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
              error_code=coap.Code.RES_NOT_FOUND)

        # delete non-existing object instance
        _delete(server, '/1234/2/2/0', error_code=coap.Code.RES_NOT_FOUND)

        # delete object instance
        _delete(server, '/1234/2')

        # second object instance doesn't exist
        _read(server, '/1234/2',
              format=coap.ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
              error_code=coap.Code.RES_NOT_FOUND)

        # delete non-existing object instance
        _delete(server, '/1234/2', error_code=coap.Code.RES_NOT_FOUND)
