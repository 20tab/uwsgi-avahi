uwsgi-avahi
===========

uWSGI plugin for Avahi integration (based on uwsgi-bonjour)

The bonjour uWSGI plugin (https://github.com/unbit/uwsgi-bonjour) allows your instances to register services in your mDNS system.

This plugin totally mimic its behavior for non-Apple systems (currently tested only on GNU/Linux).

Building it
===========

*(Be sure to have libavahi-client development headers installed)*

Just run

```sh
uwsgi --build-plugin https://github.com/20tab/uwsgi-avahi
```

the plugin will be built in the current directory *(eventually move it to your plugins directory)*

Using it
========

The same options of uwsgi-bonjour are exposed, just refer to:

https://github.com/unbit/uwsgi-bonjour/blob/master/README.md

Remember only to use avahi specific arguments instead of bonjour related:

```ini
plugins = avahi
avahi-register = darthvaderisbetterthanyoda.local
```

Notes
-----

In some GNU/linux distribution (ex: Ubuntu) we discovered some issue using `cname` parameters, use `a` instead or CNAME shortcut as in above example.
