uwsgi-avahi
===========

uWSGI plugin for avahi integration (based on uwsgi-bonjour)

The bonjour uWSGI plugin (https://github.com/unbit/uwsgi-bonjour) allows your instances to register services
in your mdns system.

This plugin totally mimic its behaviour for non-apple systems (currently tested only on Linux).

Building it
===========

(be sure to have libavahi-client development headers installed)

just run

```sh
uwsgi --build-plugin https://github.com/20tab/uwsgi-avahi
```

the plugin will be built in the current directory (eventually move it to your plugins directory)

Using it
========

The same options of uwsgi-bonjour are exposed, just refer to

https://github.com/unbit/uwsgi-bonjour/blob/master/README.md
