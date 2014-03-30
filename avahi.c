#include <uwsgi.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/error.h>
#include <avahi-common/simple-watch.h>

extern struct uwsgi_server uwsgi;

static struct uwsgi_avahi {
	struct uwsgi_string_list *record;
} uavahi;

static AvahiClient *avahi = NULL;
static AvahiEntryGroup *group = NULL;

static struct uwsgi_option avahi_options[] = {
	{"avahi-register", required_argument, 0, "register a record in the Avahi service (default: register a CNAME for the local hostname)", uwsgi_opt_add_string_list, &uavahi.record, 0},
	{"bonjour-register", required_argument, 0, "register a record in the Avahi service (default: register a CNAME for the local hostname)", uwsgi_opt_add_string_list, &uavahi.record, 0},
	{"bonjour-register-record", required_argument, 0, "register a record in the Avahi service (default: register a CNAME for the local hostname)", uwsgi_opt_add_string_list, &uavahi.record, 0},
	{"bonjour-rr", required_argument, 0, "register a record in the Avahi service (default: register a CNAME for the local hostname)", uwsgi_opt_add_string_list, &uavahi.record, 0},
	UWSGI_END_OF_OPTIONS
};

// fake functions
static void avahi_client_callback (AvahiClient *c, AvahiClientState state, void *userdata) {
}
static void avahi_entry_group_callback (AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata) {
}

// convert a c string to a dns string (each chunk is prefix with its size in uint8_t) + \0 suffix
static struct uwsgi_buffer *to_dns(char *name, size_t len) {
	uint8_t chunk_len = 0;
	size_t i;
	struct uwsgi_buffer *ub = uwsgi_buffer_new(len);
	if (!ub) goto error;
	char *ptr = name;

	for(i=0;i<len;i++) {
		if (name[i] == '.') {
			if (uwsgi_buffer_u8(ub, chunk_len)) goto error;
			if (uwsgi_buffer_append(ub, ptr,chunk_len)) goto error;
			ptr = name + i + 1;
			chunk_len = 0;
		}
		else {
			chunk_len++;	
		}
	}

	if (chunk_len > 0) {
		if (uwsgi_buffer_u8(ub, chunk_len)) goto error;
                if (uwsgi_buffer_append(ub, ptr,chunk_len)) goto error;
	}

        if (uwsgi_buffer_append(ub, "\0", 1)) goto error;

	return ub;
error:
	uwsgi_log("[uwsgi-avahi] unable to generate dns name for %s\n", name);
	exit(1);
}

static void register_cname(char *name, char *cname, int unique) {

	AvahiPublishFlags flags = unique ? AVAHI_PUBLISH_USE_MULTICAST|AVAHI_PUBLISH_UNIQUE : AVAHI_PUBLISH_USE_MULTICAST|AVAHI_PUBLISH_ALLOW_MULTIPLE;
	struct uwsgi_buffer *ub = to_dns(cname, strlen(cname));
	int ret = avahi_entry_group_add_record(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, flags, name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_CNAME, 60, ub->buf, ub->pos);
	uwsgi_buffer_destroy(ub);
	if (!ret) {
		uwsgi_log("[uwsgi-avahi] registered record %s CNAME %s\n", name, cname);
	}
	else {
		uwsgi_log_verbose("[uwsgi-avahi] avahi_entry_group_add_record(): %s\n", avahi_strerror(avahi_client_errno(avahi)));
	}
}

static void register_a(char *name, char *addr, int unique) {

	AvahiPublishFlags flags = unique ? AVAHI_PUBLISH_USE_MULTICAST|AVAHI_PUBLISH_UNIQUE : AVAHI_PUBLISH_USE_MULTICAST|AVAHI_PUBLISH_ALLOW_MULTIPLE;
	uint32_t ip = inet_addr(addr);
	int ret = avahi_entry_group_add_record(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, flags, name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_A, 60, &ip, 4);
	if (!ret) {
        	uwsgi_log("[uwsgi-avahi] registered record %s A %s\n", name, addr);
	}
	else {
		uwsgi_log_verbose("[uwsgi-avahi] avahi_entry_group_add_record(): %s\n", avahi_strerror(avahi_client_errno(avahi)));
	}
}

static void *avahi_loop(void *arg) {
	for(;;) {
		int ret = avahi_simple_poll_loop((AvahiSimplePoll *)arg);
		if (ret) {
			uwsgi_log_verbose("[uwsgi-avahi] avahi_simple_poll_loop(): %s\n", avahi_strerror(avahi_client_errno(avahi)));
			sleep(1);
		}
	}
	return NULL;
}

static void avahi_init() {

	if (!uavahi.record) return;

	AvahiSimplePoll *simple_poll = avahi_simple_poll_new();
	const AvahiPoll *poll_api = avahi_simple_poll_get(simple_poll);	
	int error;
	avahi = avahi_client_new(poll_api, 0, avahi_client_callback, NULL, &error);
	if (!avahi) {
                uwsgi_log("[uwsgi-avahi] unable to initialize DNS resolution, error: %s\n", avahi_strerror(error));
                exit(1);
	}

	const char *version = avahi_client_get_version_string (avahi);
	if (version) {
		uwsgi_log("Avahi Server Version: %s\n", version);
	}
	else {
		uwsgi_log("avahi error: %s\n", avahi_strerror(avahi_client_errno(avahi)));
	}

	group = avahi_entry_group_new(avahi, avahi_entry_group_callback, NULL);
	if (!group) {
		uwsgi_log("[uwsgi-avahi] unable to initialize entry group: %s\n", avahi_strerror(avahi_client_errno (avahi)));
		exit(1);
	}

	char *myself = uwsgi.hostname;
	if (!uwsgi_endswith(myself, ".local") && !uwsgi_endswith(myself, ".lan")) {
		myself = uwsgi_concat2(uwsgi.hostname, ".local");
	}

	struct uwsgi_string_list *usl = NULL;
	uwsgi_foreach(usl, uavahi.record) {
		char *b_name = NULL;
		char *b_cname = NULL;
		char *b_unique = NULL;
		char *b_ip = NULL;
		if (strchr(usl->value, '=')) {
			if (uwsgi_kvlist_parse(usl->value, usl->len, ',', '=',
			"name", &b_name,
			"cname", &b_cname,
			"unique", &b_unique,
			"ip", &b_ip,
			"a", &b_ip,
			NULL)) {
				uwsgi_log("[uwsgi-avahi] invalid keyval syntax\n");
				exit(1);
			}

			if (!b_name) {
				uwsgi_log("[uwsgi-avahi] you need to specify the name key to register a record\n");
				exit(1);
			}
			
			if (b_cname) {
				register_cname(b_name, b_cname, b_unique ? 1 : 0);	
			}
			else if (b_ip) {
				register_a(b_name, b_ip, b_unique ? 1 : 0);	
			}
			else {
				register_cname(b_name, myself, b_unique ? 1 : 0);
			}

			if (b_name) free(b_name);
			if (b_cname) free(b_cname);
			if (b_unique) free(b_unique);
			if (b_ip) free(b_ip);
		}
		else {
			register_cname(usl->value, myself, 0);
		}
	}

	int ret = avahi_entry_group_commit(group);
	if (ret) {
		uwsgi_log("[uwsgi-avahi] error committing records: %s\n", avahi_strerror(avahi_client_errno(avahi)));
		exit(1);
	}

	if (myself != uwsgi.hostname)
		free(myself);

	// now start a pthread for managing avahi
	pthread_t t;
        pthread_create(&t, NULL, avahi_loop, (void *)poll_api);
}

struct uwsgi_plugin avahi_plugin = {
	.name = "avahi",
	.options = avahi_options,
	// we use .post_init instead of .init to avoid the mdsn socket to be closed on reloads
	.post_init = avahi_init,
};
