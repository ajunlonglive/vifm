#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static int counter;

TEST(allow_user_key_remap)
{
	assert_int_equal(0, add_user_keys(L"jo", L":do movement", NORMAL_MODE, 0));
	assert_int_equal(0,
			add_user_keys(L"jo", L":leave insert mode", NORMAL_MODE, 0));
}

static int
handler(wchar_t key)
{
	static const wchar_t line[] = L"toto";

	char exp[] = {line[counter++], '\0'};
	char act[] = {key, '\0'};

	assert_string_equal(exp, act);
	return 0;
}

TEST(prevent_stack_overflow)
{
	assert_int_equal(0, add_user_keys(L"j", L"j", NORMAL_MODE, 0));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"j")));

	assert_int_equal(0, add_user_keys(L"q", L"q", NORMAL_MODE, 0));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"q"));

	set_def_handler(NORMAL_MODE, handler);

	assert_int_equal(0, add_user_keys(L"t", L"toto", NORMAL_MODE, 0));
	assert_int_equal(0, execute_keys(L"t"));

	assert_int_equal(4, counter);

	set_def_handler(NORMAL_MODE, NULL);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
