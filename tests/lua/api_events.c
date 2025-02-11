#include <stic.h>

#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/utils/str.h"

#include <test-utils.h>

static vlua_t *vlua;

SETUP()
{
	vlua = vlua_init();
}

TEARDOWN()
{
	vlua_finish(vlua);
}

TEST(bad_event_name)
{
	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua,
	      "vifm.events.listen {"
	      "  event = 'random',"
	      "  handler = function() end"
	      "}"));
	assert_true(ends_with(ui_sb_last(), ": No such event: random"));
}

TEST(app_exit_is_called)
{
	assert_success(vlua_run_string(vlua,
	      "i = 0\n"
	      "vifm.events.listen {"
	      "  event = 'app.exit',"
	      "  handler = function() i = i + 1 end"
	      "}"));

	vlua_events_app_exit(vlua);
	vlua_process_callbacks(vlua);

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(i)"));
	assert_string_equal("1", ui_sb_last());
}

TEST(same_handler_is_called_once)
{
	ui_sb_msg("");

	assert_success(vlua_run_string(vlua,
	      "i = 0\n"
	      "handler = function() i = i + 1 end\n"
	      "vifm.events.listen { event = 'app.exit', handler = handler }"
	      "vifm.events.listen { event = 'app.exit', handler = handler }"));

	vlua_events_app_exit(vlua);
	vlua_process_callbacks(vlua);
	assert_string_equal("", ui_sb_last());

	assert_success(vlua_run_string(vlua, "print(i)"));
	assert_string_equal("1", ui_sb_last());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
