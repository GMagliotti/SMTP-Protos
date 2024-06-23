
#include "process.h"

#include "smtp.h"
#include "states.h"

#define NUM_COMMANDS (sizeof(valid_commands) / sizeof(valid_commands[0]))
static bool extract_email(char* arg, char* email, size_t email_len);

static const char* valid_commands[] = {
	HELO_VERB, EHLO_VERB, XADM_VERB, MAIL_VERB, RCPT_VERB, DATA_VERB,
};

static bool
is_valid(char* verb, char* state_verb, char* msg)
{
	if (strcasecmp(verb, state_verb) != 0) {
		for (size_t i = 0; i < NUM_COMMANDS; i++) {
			if (strcasecmp(verb, valid_commands[i]) == 0) {
				// es comando pero no en secuencia
				sprintf(msg, "503 5.5.1 Error: Bad Command Sequence");
				return false;
			}
		}
		sprintf(msg, "502 5.5.2 Error: Command Not Recognized");
		return false;
	}
	return true;
}

smtp_state
handle_helo(struct selector_key* key, bool* error, char* msg)
{
	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;
	if (strcasecmp(verb, HELO_VERB) != 0 && strcasecmp(verb, EHLO_VERB) != 0) {
		sprintf(msg, "502 5.5.2 Error: Command Not Recognized");  /// TODO:

		return EHLO;
	}
	char* arg = data->request.arg;
	if (arg == NULL || *arg == '\0') {
		// msg = state_table[FROM].error_msg;
		sprintf(msg, "502 5.5.2 Error: Command Not Recognized");  // TODO :

		return EHLO;
	}
	// msg = state_table[FROM].success_msg;
	*error = false;
	sprintf(msg, "TODO EN ARGY ES UNA JODA [EHLO]");  // TODO :

	return EHLO_DONE;
}
smtp_state
handle_xadm(struct selector_key* key, bool* error, char* msg)
{
	*error = false;

	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;
	if (strcasecmp(verb, XADM_VERB) != 0) {
		sprintf(msg, "502 5.5.2 Error: Command Not Recognized");  // TODO :
		*error = true;
		return EHLO;
	}

	return EHLO;
}
smtp_state
handle_from(struct selector_key* key, bool* error, char* msg)
{
	*error = false;

	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;

	if (!is_valid(verb, MAIL_VERB, msg)) {
		return FROM;
	}

	char* arg = data->request.arg;

	if (strncasecmp(arg, FROM_PREFIX, strlen(FROM_PREFIX)) != 0) {
		// check arg prefix
		sprintf(msg, "501 5.5.4 Syntax: MAIL FROM:<address>");
		return FROM;
	}

	arg += strlen(FROM_PREFIX);
	char mail[MAIL_SIZE];

	// check mail and extract
	bool valid = extract_email(arg, mail, MAIL_SIZE);
	if (!valid) {
		sprintf(msg, "501 5.5.4 Syntax: MAIL FROM:<address>");
		return FROM;
	}
	strcpy((char*)data->mail_from, mail);
	sprintf(msg, "TODO EN ARGY ES UNA JODA [FROM]");  // TODO :

	return TO;
}

smtp_state
handle_ehlo_done(struct selector_key* key, bool* error, char* msg)
{
	*error = false;

	smtp_state ret = handle_from(key, error, msg);

	if (ret == EHLO && !*error) {
		ret = handle_xadm(key, error, msg);
	}

	return ret;
}
smtp_state
handle_to(struct selector_key* key, bool* error, char* msg)
{
	*error = false;

	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;

	if (!is_valid(verb, RCPT_VERB, msg)) {
		return TO;
	}

	char* arg = data->request.arg;

	if (((char*)data->mail_from)[0] != '\0') {
		sprintf(msg, "503 5.5.1 Error: nested MAIL command");
		return TO;
	}

	if (strncasecmp(arg, TO_PREFIX, strlen(TO_PREFIX)) != 0) {
		sprintf(msg, "501 5.1.3 Bad recipient address syntax");
		return TO;
	}

	arg += strlen(TO_PREFIX);
	char mail[MAIL_SIZE];
	bool valid = extract_email(arg, mail, MAIL_SIZE);
	if (!valid) {
		sprintf(msg, "501 5.1.3 Bad recipient address syntax");
		return TO;
	}
	strcpy((char*)data->rcpt_to, mail);
	sprintf(msg, "TODO EN ARGY ES UNA JODA [TO]");  // TODO :

	return DATA;  // TODO Resolver no deterministico
}

static bool
extract_email(char* arg, char* email, size_t email_len)
{
	const char* start = strchr(arg, '<');
	const char* end = strrchr(arg, '>');
	if (start != NULL && end != NULL && start == arg && end == (start + strlen(arg) - 1)) {
		size_t len = end - start - 1;
		if (len < email_len) {
			strncpy(email, start + 1, len);
			email[len] = '\0';
			return true;
		}
	}
	return false;
}

smtp_state
handle_body(struct selector_key* key, bool* error, char* msg)
{
	// ya tengo todo el body del mail.

	smtp_data* data = ATTACHMENT(key);
	*error = false;
	// sprintf(msg, "501 5.1.3 Bad recipient address syntax");  // TODO NO est abien
	char* body = (char*)data->data;

	sprintf(msg, "MAIL SENT, AGUANTE ARGENTINA !");  // TODO NO est abien
	strcpy((char*)body, data->request.data);

	// mandar el mail
	memset(&data->request, 0, sizeof((data->request)));
	memset(&data->mail_from, 0, sizeof((data->mail_from)));
	memset(&data->data, 0, sizeof((data->data)));
	memset(&data->rcpt_to, 0, sizeof((data->rcpt_to)));

	data->state = EHLO;
	return EHLO;
}

smtp_state
handle_data(struct selector_key* key, bool* error, char* msg)
{
	*error = false;

	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;

	if (strcasecmp(verb, RCPT_VERB) == 0) {
		smtp_state ret = handle_to(key, error, msg);  // lidio con la no determinacion
		if (ret == TO) {
			return DATA;
		}
	}

	if (strcasecmp(verb, DATA_VERB) != 0) {
		sprintf(msg, "TODO EN ARGY ES UNA JODA [DATA ERROR]");  // TODO :
		// msg = state_table->success_msg;
		return DATA;
	}
	sprintf(msg, "354 End data with <CR><LF>.<CR><LF>");  // TODO :

	return BODY;
}
