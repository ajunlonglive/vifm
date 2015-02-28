#include <stic.h>

#include <stdint.h> /* uint64_t */
#include <stdio.h> /* remove() */

#include <unistd.h> /* F_OK access() */

#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/utils.h"

#include "utils.h"

static int not_windows(void);
static int windows(void);

TEST(file_is_moved)
{
	create_empty_file("binary-data");

	{
		io_args_t args =
		{
			.arg1.src = "binary-data",
			.arg2.dst = "moved-binary-data",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_false(access("binary-data", F_OK) == 0);
	assert_int_equal(0, access("moved-binary-data", F_OK));

	remove("moved-binary-data");
}

TEST(empty_directory_is_moved)
{
	create_empty_dir("empty-dir");

	{
		io_args_t args =
		{
			.arg1.src = "empty-dir",
			.arg2.dst = "moved-empty-dir",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_false(is_dir("empty-dir"));
	assert_true(is_dir("moved-empty-dir"));

	{
		io_args_t args =
		{
			.arg1.path = "moved-empty-dir",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}
}

TEST(non_empty_directory_is_moved)
{
	create_non_empty_dir("non-empty-dir", "a-file");

	{
		io_args_t args =
		{
			.arg1.src = "non-empty-dir",
			.arg2.dst = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_int_equal(0, access("moved-non-empty-dir/a-file", F_OK));

	{
		io_args_t args =
		{
			.arg1.path = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

TEST(empty_nested_directory_is_moved)
{
	create_empty_nested_dir("non-empty-dir", "empty-nested-dir");

	{
		io_args_t args =
		{
			.arg1.src = "non-empty-dir",
			.arg2.dst = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_true(is_dir("moved-non-empty-dir/empty-nested-dir"));

	{
		io_args_t args =
		{
			.arg1.path = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

TEST(non_empty_nested_directory_is_moved)
{
	create_non_empty_nested_dir("non-empty-dir", "nested-dir", "a-file");

	{
		io_args_t args =
		{
			.arg1.src = "non-empty-dir",
			.arg2.dst = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_false(access("non-empty-dir/nested-dir/a-file", F_OK) == 0);
	assert_int_equal(0, access("moved-non-empty-dir/nested-dir/a-file", F_OK));

	{
		io_args_t args =
		{
			.arg1.path = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

TEST(fails_to_overwrite_file_by_default)
{
	create_empty_file("a-file");

	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "a-file",
		};
		assert_false(ior_mv(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "a-file",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

TEST(fails_to_overwrite_dir_by_default)
{
	create_empty_dir("empty-dir");

	{
		io_args_t args =
		{
			.arg1.src = "../read",
			.arg2.dst = "empty-dir",
		};
		assert_false(ior_mv(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "empty-dir",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}
}

TEST(overwrites_file_when_asked)
{
	create_empty_file("a-file");

	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "two-lines",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.src = "two-lines",
			.arg2.dst = "a-file",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		assert_int_equal(0, ior_mv(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "a-file",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

TEST(overwrites_dir_when_asked)
{
	create_empty_dir("dir");

	{
		io_args_t args =
		{
			.arg1.src = "../read",
			.arg2.dst = "read",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.src = "read",
			.arg2.dst = "dir",
			.arg3.crs = IO_CRS_REPLACE_ALL,
		};
		assert_int_equal(0, ior_mv(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "dir",
		};
		assert_false(iop_rmdir(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

TEST(appending_fails_for_directories)
{
	create_empty_dir("dir");

	{
		io_args_t args =
		{
			.arg1.src = "../read",
			.arg2.dst = "read",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.src = "read",
			.arg2.dst = "dir",
			.arg3.crs = IO_CRS_APPEND_TO_FILES,
		};
		assert_false(ior_mv(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "dir",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "read",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

TEST(appending_works_for_files)
{
	uint64_t size;

	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "two-lines",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	size = get_file_size("two-lines");

	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "two-lines2",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.src = "two-lines2",
			.arg2.dst = "two-lines",
			.arg3.crs = IO_CRS_APPEND_TO_FILES,
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_int_equal(size, get_file_size("two-lines"));

	{
		io_args_t args =
		{
			.arg1.path = "two-lines",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

TEST(directories_can_be_merged)
{
	create_empty_dir("first");

	assert_int_equal(0, chdir("first"));
	create_empty_file("first-file");
	assert_int_equal(0, chdir(".."));

	create_empty_dir("second");

	assert_int_equal(0, chdir("second"));
	create_empty_file("second-file");
	assert_int_equal(0, chdir(".."));

	{
		io_args_t args =
		{
			.arg1.src = "first",
			.arg2.dst = "second",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_int_equal(0, access("second/second-file", F_OK));
	assert_int_equal(0, access("second/first-file", F_OK));

	{
		io_args_t args =
		{
			.arg1.path = "first",
		};
		assert_int_equal(0, ior_rm(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "second",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

TEST(fails_to_move_directory_inside_itself)
{
	create_empty_dir("empty-dir");

	{
		io_args_t args =
		{
			.arg1.src = "empty-dir",
			.arg2.dst = "empty-dir/empty-dir-copy",
		};
		assert_false(ior_mv(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "empty-dir",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}
}

/* Creating symbolic links on Windows requires administrator rights. */
TEST(symlink_is_symlink_after_move, IF(not_windows))
{
	{
		io_args_t args =
		{
			.arg1.path = "../read/two-lines",
			.arg2.target = "sym-link",
		};
		assert_int_equal(0, iop_ln(&args));
	}

	assert_true(is_symlink("sym-link"));

	{
		io_args_t args =
		{
			.arg1.src = "sym-link",
			.arg2.dst = "moved-sym-link",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_true(!is_symlink("sym-link"));
	assert_true(is_symlink("moved-sym-link"));

	{
		io_args_t args =
		{
			.arg1.path = "moved-sym-link",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

/* Case insensitive renames are easier to check on Windows. */
TEST(case_insensitive_rename, IF(windows))
{
	create_empty_file("a-file");

	{
		io_args_t args =
		{
			.arg1.src = "a-file",
			.arg2.dst = "A-file",
		};
		assert_true(ior_mv(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "A-file",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

static int
not_windows(void)
{
	return get_env_type() != ET_WIN;
}

static int
windows(void)
{
	return get_env_type() == ET_WIN;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
