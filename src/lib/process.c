
#include "process.h"

#include "access_registry.h"
#include "smtp.h"
#include "states.h"

#include <time.h>

#define NUM_COMMANDS (sizeof(valid_commands) / sizeof(valid_commands[0]))
#define OK_RCPT      "2.1.0"
#define OK_MAIL      "2.1.0"
#define OK_RSET      "2.0.0"
#define OK_NOOP      "2.0.0"
static bool extract_email(char* arg, char* email, size_t email_len);
static void bad_sequence(char* buf);
static void bad_command(char* buf);
static void bad_syntax(char* buf, char* syntax);
static void bad_pwd(char* buf);
static void ok(char* buf, char* code);
static void welcome(char* buf);
static void ok_data(char* buf);
static void ok_body(char* buf);
static bool is_valid(char* verb, char* state_verb, char* msg);
static void bad_user(char* buf);
// static void clean_request(struct selector_key* key);
static void auth_msg(char* buf);

static const char* valid_commands[] = { HELO_VERB,  EHLO_VERB,  MAIL_VERB, RCPT_VERB, DATA_VERB,
	                                    XFROM_VERB, XAUTH_VERB, XGET_VERB, XQUIT_VERB };

bool
handle_reset(struct selector_key* key, char* msg)
{
	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;
	if (strcasecmp(verb, RSET_VERB) != 0) {
		return false;
	}
	char* arg = data->request.arg;
	if (arg != NULL && *arg != '\0') {
		bad_syntax(msg, "RSET");
		return true;
	}
	// msg = state_table[FROM].success_msg;
	ok(msg, OK_RSET);
	data->state = FROM;
	return true;
}

bool
handle_noop(struct selector_key* key, char* msg)
{
	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;
	if (strcasecmp(verb, NOOP_VERB) != 0) {
		return false;
	}
	ok(msg, OK_NOOP);
	return true;
}

bool
handle_xquit(struct selector_key* key, char* msg)
{
	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;

	if (strcasecmp(verb, XQUIT_VERB) == 0 && (data->state == XFROM || data->state == XGET)) {
		ok(msg, "XQUIT!");
		data->state = FROM;
		return true;
	}

	return false;
}

smtp_state
handle_helo(struct selector_key* key, char* msg)
{
	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;
	if (strcasecmp(verb, HELO_VERB) != 0 && strcasecmp(verb, EHLO_VERB) != 0) {
		bad_command(msg);

		return EHLO;
	}
	char* arg = data->request.arg;
	if (arg == NULL || *arg == '\0') {
		// msg = state_table[FROM].error_msg;
		bad_command(msg);
		return EHLO;
	}
	// msg = state_table[FROM].success_msg;

	welcome(msg);

	return FROM;
}

smtp_state
handle_from(struct selector_key* key, char* msg)
{
	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;

	if (strcasecmp(verb, XAUTH_VERB) == 0) {
		return handle_xauth(key, msg);  // lidio con la no determinacion
	}

	if (!is_valid(verb, MAIL_VERB, msg)) {
		return FROM;
	}

	char* arg = data->request.arg;

	if (strncasecmp(arg, FROM_PREFIX, strlen(FROM_PREFIX)) != 0) {
		// check arg prefix
		bad_syntax(msg, "MAIL FROM:<address>");
		return FROM;
	}

	arg += strlen(FROM_PREFIX);
	char mail[MAIL_SIZE];

	// check mail and extract
	bool valid = extract_email(arg, mail, MAIL_SIZE);
	if (!valid) {
		bad_syntax(msg, "MAIL FROM:<address>");
		return FROM;
	}

	strcpy((char*)data->mail_from, mail);
	ok(msg, OK_MAIL);

	return TO;
}

smtp_state
handle_to(struct selector_key* key, char* msg)
{
	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;

	if (!is_valid(verb, RCPT_VERB, msg)) {
		return TO;
	}

	char* arg = data->request.arg;

	if (strncasecmp(arg, TO_PREFIX, strlen(TO_PREFIX)) != 0) {
		bad_syntax(msg, "RCPT TO:<address>");
		return TO;
	}

	arg += strlen(TO_PREFIX);
	char mail[MAIL_SIZE];
	bool valid = extract_email(arg, mail, MAIL_SIZE);
	if (!valid) {
		bad_syntax(msg, "RCPT TO:<address>");
		return TO;
	}
	strcpy((char*)data->rcpt_to[data->rcpt_qty++], mail);

	ok(msg, OK_RCPT);

	return DATA;  // TODO Resolver no deterministico
}

smtp_state
handle_body(struct selector_key* key, char* msg)
{
	// ya tengo todo el body del mail.

	smtp_data* data = ATTACHMENT(key);
	// sprintf(msg, "501 5.1.3 Bad recipient address syntax");  // TODO NO est abien
	char* body = (char*)data->data;

	ok_body(msg);
	strcpy((char*)body, data->request.data);

	data->state = EHLO;
	return EHLO;
}

smtp_state
handle_data(struct selector_key* key, char* msg)
{
	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;

	if (strcasecmp(verb, RCPT_VERB) == 0) {
		smtp_state ret = handle_to(key, msg);  // lidio con la no determinacion
		if (ret == DATA) {
			return DATA;
		}
	}

	if (strcasecmp(verb, DATA_VERB) != 0) {
		bad_command(msg);  // TODO :
		// msg = state_table->success_msg;
		return DATA;
	}
	ok_data(msg);

	return BODY;
}

smtp_state
handle_xauth(struct selector_key* key, char* msg)
{
	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;

	if (!is_valid(verb, XAUTH_VERB, msg)) {
		return FROM;
	}

	char* arg = data->request.arg;
	if (arg == NULL || *arg == '\0') {
		bad_syntax(msg, "AUTH <password>");
		return true;
	}
	if (!authenticate(arg)) {
		bad_pwd(msg);
		return FROM;
	}

	auth_msg(msg);

	return XFROM;
}
smtp_state
handle_xfrom(struct selector_key* key, char* msg)
{
	/*
	    Dispuse que este comando tiene un argumento:

	    * Un USER válido


	*/
	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;

	if (!is_valid(verb, XFROM_VERB, msg)) {
		return XFROM;
	}

	char* arg = data->request.arg;

	// if (!is_user(arg)) {
	bad_user(msg);
	// return XFROM;
	// }

	strcpy((char*)data->user, arg);

	ok(msg, "XFROM!");

	return XGET;
}
smtp_state
handle_xget(struct selector_key* key, char* msg)
{
	/*
	    Dispuse que este comando tiene dos argumentos posibles:

	    * ALL
	    * una fecha en formato dd/mm/yyyy


	*/

	smtp_data* data = ATTACHMENT(key);
	char* verb = data->request.verb;

	if (strcasecmp(verb, XFROM_VERB) == 0) {
		return handle_xfrom(key, msg);  // lidio con la no determinacion
	}

	if (!is_valid(verb, XGET_VERB, msg)) {
		return XGET;
	}

	char* arg = data->request.arg;

	time_t time;

	if (strcasecmp(arg, XGET_ALL) == 0) {
		sprintf(msg, "XGET ALL");
		// ver que ponemos en el request
		// print_mails(data->fd, (char*)data->user);

	} else if (convert_and_validate_date(arg, &time)) {
		sprintf(msg, "XGET %s", arg);
		// ver que ponemos en el request
		// print_mails_by_day(data->fd, time);

	} else {
		bad_syntax(msg, "XGET <date> | XGET ALL");
		return XGET;
	}

	// realizar el
	memset(&data->user, 0, sizeof(data->user));

	ok(msg, "XGET!");

	return XFROM;
}

static void
auth_msg(char* buf)
{
	sprintf(buf, "ADMIN authenticated! \n");
}

static void
bad_pwd(char* buf)
{
	sprintf(buf, "[CODE] Incorrect Password! \n");
}

// static void
// clean_request(struct selector_key* key)
// {
// 	smtp_data* data = ATTACHMENT(key);
// 	memset(&data->request, 0, sizeof((data->request)));
// 	memset(&data->mail_from, 0, sizeof((data->mail_from)));
// 	memset(&data->data, 0, sizeof((data->data)));
// 	memset(&data->rcpt_to, 0, sizeof((data->rcpt_to)));
// }

static void
bad_sequence(char* buf)
{
	sprintf(buf, "503 5.5.1 Error: Bad Command Sequence \n");
}
static void
bad_user(char* buf)
{
	sprintf(buf, "XFROM: Bad User \n");
}

static void
bad_command(char* buf)
{
	sprintf(buf, "502 5.5.2 Error: Command Not Recognized\n");
}
static void
bad_syntax(char* buf, char* syntax)
{
	sprintf(buf, "501 5.5.4 Syntax: %s \n", syntax);
}

static void
ok(char* buf, char* code)
{
	sprintf(buf, "250 %s Ok\n", code);
}

static void
welcome(char* buf)
{
	sprintf(buf, "250-welcome message (capacidades podríamos poner) \n");
}

static void
ok_data(char* buf)
{
	sprintf(buf, "354 End data with <CR><LF>.<CR><LF> \n");
}

static void
ok_body(char* buf)
{
	sprintf(buf, "250 2.0.0 Ok: (queued as ?) EMAIL SENT \n");
}

static bool
is_valid(char* verb, char* state_verb, char* msg)
{
	if (strcasecmp(verb, state_verb) != 0) {
		for (size_t i = 0; i < NUM_COMMANDS; i++) {
			if (strcasecmp(verb, valid_commands[i]) == 0) {
				// es comando pero no en secuencia
				bad_sequence(msg);
				return false;
			}
		}
		bad_command(msg);
		return false;
	}
	return true;
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
