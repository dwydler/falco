// SPDX-License-Identifier: Apache-2.0
/*
Copyright (C) 2023 The Falco Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <gtest/gtest.h>
#include <falco/configuration.h>

#ifdef _WIN32
#define SET_ENV_VAR(env_var_name, env_var_value) _putenv_s(env_var_name, env_var_value)
#else
#define SET_ENV_VAR(env_var_name, env_var_value) setenv(env_var_name, env_var_value, 1)
#endif

static std::string sample_yaml =
	"base_value:\n"
	"    id: 1\n"
	"    name: 'sample_name'\n"
	"    subvalue:\n"
	"      subvalue2:\n"
	"        boolean: true\n"
	"base_value_2:\n"
	"  sample_list:\n"
	"    - elem1\n"
	"    - elem2\n"
	"    - elem3\n";

TEST(Configuration, configuration_exceptions)
{
	yaml_helper conf;

	/* Broken YAML */
	std::string sample_broken_yaml = sample_yaml + " /  bad_symbol";
	EXPECT_ANY_THROW(conf.load_from_string(sample_broken_yaml));

	/* Right YAML */
	EXPECT_NO_THROW(conf.load_from_string(sample_yaml));
}

TEST(Configuration, configuration_reload)
{
	yaml_helper conf;

	/* Clear and reload config */
	conf.load_from_string(sample_yaml);
	ASSERT_TRUE(conf.is_defined("base_value"));
	conf.clear();
	ASSERT_FALSE(conf.is_defined("base_value"));
	conf.load_from_string(sample_yaml);
	ASSERT_TRUE(conf.is_defined("base_value"));
}

TEST(Configuration, read_yaml_fields)
{
	yaml_helper conf;
	conf.load_from_string(sample_yaml);

	/* is_defined */
	ASSERT_TRUE(conf.is_defined("base_value"));
	ASSERT_TRUE(conf.is_defined("base_value_2"));
	ASSERT_FALSE(conf.is_defined("unknown_base_value"));

	/* get some fields */
	ASSERT_EQ(conf.get_scalar<int>("base_value.id", -1), 1);
	ASSERT_STREQ(conf.get_scalar<std::string>("base_value.name", "none").c_str(), "sample_name");
	ASSERT_EQ(conf.get_scalar<bool>("base_value.subvalue.subvalue2.boolean", false), true);

	/* get list field elements */
	ASSERT_STREQ(conf.get_scalar<std::string>("base_value_2.sample_list[0]", "none").c_str(), "elem1");
	ASSERT_STREQ(conf.get_scalar<std::string>("base_value_2.sample_list[1]", "none").c_str(), "elem2");
	ASSERT_STREQ(conf.get_scalar<std::string>("base_value_2.sample_list[2]", "none").c_str(), "elem3");

	/* get sequence */
	std::vector<std::string> seq;
	conf.get_sequence(seq, "base_value_2.sample_list");
	ASSERT_EQ(seq.size(), 3);
	ASSERT_STREQ(seq[0].c_str(), "elem1");
	ASSERT_STREQ(seq[1].c_str(), "elem2");
	ASSERT_STREQ(seq[2].c_str(), "elem3");
}

TEST(Configuration, modify_yaml_fields)
{
	std::string key = "base_value.subvalue.subvalue2.boolean";
	yaml_helper conf;

    /* Get original value */
    conf.load_from_string(sample_yaml);
	ASSERT_EQ(conf.get_scalar<bool>(key, false), true);

    /* Modify the original value */
    conf.set_scalar<bool>(key, false);
	ASSERT_EQ(conf.get_scalar<bool>(key, true), false);

    /* Modify it again */
    conf.set_scalar<bool>(key, true);
	ASSERT_EQ(conf.get_scalar<bool>(key, false), true);
}

TEST(Configuration, configuration_config_files_secondary_fail)
{
	/* Test that a secondary config file is not able to include anything, triggering an exception. */
	const std::string main_conf_yaml =
		yaml_helper::configs_key + ":\n"
					   "  - conf_2.yaml\n"
					   "  - conf_3.yaml\n"
					   "foo: bar\n"
					   "base_value:\n"
					   "    id: 1\n"
					   "    name: foo\n";
	const std::string conf_yaml_2 =
		yaml_helper::configs_key + ":\n"
					   "  - conf_4.yaml\n"
					   "foo2: bar2\n"
					   "base_value_2:\n"
					   "    id: 2\n";

	std::ofstream outfile("main.yaml");
	outfile << main_conf_yaml;
	outfile.close();

	outfile.open("conf_2.yaml");
	outfile << conf_yaml_2;
	outfile.close();

	std::vector<std::string> cmdline_config_options;
	std::vector<std::string> loaded_conf_files;
	falco_configuration falco_config;
	ASSERT_ANY_THROW(falco_config.init("main.yaml", loaded_conf_files, cmdline_config_options));

	std::filesystem::remove("main.yaml");
	std::filesystem::remove("conf_2.yaml");
}

TEST(Configuration, configuration_config_files_ok)
{
	/* Test that every included config file was correctly parsed */
	const std::string main_conf_yaml =
		yaml_helper::configs_key + ":\n"
					   "  - conf_2.yaml\n"
					   "  - conf_3.yaml\n"
					   "foo: bar\n"
					   "base_value:\n"
					   "    id: 1\n"
					   "    name: foo\n";
	const std::string conf_yaml_2 =
		"foo2: bar2\n"
		"base_value_2:\n"
		"    id: 2\n";
	const std::string conf_yaml_3 =
		"foo3: bar3\n"
		"base_value_3:\n"
		"    id: 3\n"
		"    name: foo3\n";
	const std::string conf_yaml_4 =
		"base_value_4:\n"
		"    id: 4\n";

	std::ofstream outfile("main.yaml");
	outfile << main_conf_yaml;
	outfile.close();

	outfile.open("conf_2.yaml");
	outfile << conf_yaml_2;
	outfile.close();

	outfile.open("conf_3.yaml");
	outfile << conf_yaml_3;
	outfile.close();

	outfile.open("conf_4.yaml");
	outfile << conf_yaml_4;
	outfile.close();

	std::vector<std::string> cmdline_config_options;
	std::vector<std::string> loaded_conf_files;
	falco_configuration falco_config;
	ASSERT_NO_THROW(falco_config.init("main.yaml", loaded_conf_files, cmdline_config_options));

	// main + conf_2 + conf_3
	ASSERT_EQ(loaded_conf_files.size(), 3);

	ASSERT_TRUE(falco_config.config.is_defined("foo"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("foo", ""), "bar");
	ASSERT_TRUE(falco_config.config.is_defined("base_value.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value.id", 0), 1);
	ASSERT_TRUE(falco_config.config.is_defined("base_value.name"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("base_value.name", ""), "foo");
	ASSERT_TRUE(falco_config.config.is_defined("foo2"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("foo2", ""), "bar2");
	ASSERT_TRUE(falco_config.config.is_defined("base_value_2.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value_2.id", 0), 2);
	ASSERT_TRUE(falco_config.config.is_defined("foo3"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("foo3", ""), "bar3");
	ASSERT_TRUE(falco_config.config.is_defined("base_value_3.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value_3.id", 0), 3);
	ASSERT_TRUE(falco_config.config.is_defined("base_value_3.name"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("base_value_3.name", ""), "foo3");
	ASSERT_FALSE(falco_config.config.is_defined("base_value_4.id")); // conf_4 is not included

	std::filesystem::remove("main.yaml");
	std::filesystem::remove("conf_2.yaml");
	std::filesystem::remove("conf_3.yaml");
	std::filesystem::remove("conf_4.yaml");
}


TEST(Configuration, configuration_config_files_relative_main)
{
	/*
	 * Test that relative path are treated as relative to cwd and not to main config folder,
	 * and that absolute includes are ok too.
	 */
	const auto temp_main = std::filesystem::temp_directory_path() / "main.yaml";
	// So, conf_2 will be looked up in the same folder as main config file,
	// while conf_3, since is absolute, will be looked up in the absolute path (and found!).
	const std::string main_conf_yaml =
		yaml_helper::configs_key + ":\n"
		"  - conf_2.yaml\n"
		"  - " +
		std::filesystem::current_path().string() + "/conf_3.yaml\n"
							   "foo: bar\n"
							   "base_value:\n"
							   "    id: 1\n"
							   "    name: foo\n";
	const std::string conf_yaml_2 =
		"foo2: bar2\n"
		"base_value_2:\n"
		"    id: 2\n";
	const std::string conf_yaml_3 =
		"foo3: bar3\n"
		"base_value_3:\n"
		"    id: 3\n"
		"    name: foo3\n";

	std::ofstream outfile(temp_main.string());
	outfile << main_conf_yaml;
	outfile.close();

	outfile.open("conf_2.yaml");
	outfile << conf_yaml_2;
	outfile.close();

	outfile.open("conf_3.yaml");
	outfile << conf_yaml_3;
	outfile.close();

	std::vector<std::string> cmdline_config_options;
	std::vector<std::string> loaded_conf_files;
	falco_configuration falco_config;
	ASSERT_NO_THROW(falco_config.init(temp_main.string(), loaded_conf_files, cmdline_config_options));

	// main + conf_2 + conf_3
	ASSERT_EQ(loaded_conf_files.size(), 3);

	ASSERT_TRUE(falco_config.config.is_defined("foo"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("foo", ""), "bar");
	ASSERT_TRUE(falco_config.config.is_defined("base_value.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value.id", 0), 1);
	ASSERT_TRUE(falco_config.config.is_defined("base_value.name"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("base_value.name", ""), "foo");
	ASSERT_TRUE(falco_config.config.is_defined("foo2"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("foo2", ""), "bar2");
	ASSERT_TRUE(falco_config.config.is_defined("base_value_2"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value_2.id", 0), 2);
	ASSERT_TRUE(falco_config.config.is_defined("base_value_3.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value_3.id", 0), 3);

	std::filesystem::remove(temp_main.string());
	std::filesystem::remove("conf_2.yaml");
	std::filesystem::remove("conf_3.yaml");
}

TEST(Configuration, configuration_config_files_override)
{
	/* Test that included config files are able to override configs from main file */
	const std::string main_conf_yaml =
		yaml_helper::configs_key + ":\n"
					   "  - conf_2.yaml\n"
					   "  - conf_3.yaml\n"
					   "foo: bar\n"
					   "base_value:\n"
					   "    id: 1\n"
					   "    name: foo\n";
	const std::string conf_yaml_2 =
		"foo2: bar2\n"
		"base_value_2:\n"
		"    id: 2\n";
	const std::string conf_yaml_3 =
		"base_value:\n"
		"    id: 3\n";

	std::ofstream outfile("main.yaml");
	outfile << main_conf_yaml;
	outfile.close();

	outfile.open("conf_2.yaml");
	outfile << conf_yaml_2;
	outfile.close();

	outfile.open("conf_3.yaml");
	outfile << conf_yaml_3;
	outfile.close();

	std::vector<std::string> cmdline_config_options;
	std::vector<std::string> loaded_conf_files;
	falco_configuration falco_config;
	ASSERT_NO_THROW(falco_config.init("main.yaml", loaded_conf_files, cmdline_config_options));

	// main + conf_2 + conf_3
	ASSERT_EQ(loaded_conf_files.size(), 3);

	ASSERT_TRUE(falco_config.config.is_defined("foo"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("foo", ""), "bar");
	ASSERT_TRUE(falco_config.config.is_defined("base_value.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value.id", 0), 3); // overridden!
	ASSERT_FALSE(falco_config.config.is_defined("base_value.name"));	// no more present since entire `base_value` block was overridden
	ASSERT_TRUE(falco_config.config.is_defined("foo2"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("foo2", ""), "bar2");
	ASSERT_TRUE(falco_config.config.is_defined("base_value_2.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value_2.id", 0), 2);
	ASSERT_FALSE(falco_config.config.is_defined("base_value_3.id")); // not defined

	std::filesystem::remove("main.yaml");
	std::filesystem::remove("conf_2.yaml");
	std::filesystem::remove("conf_3.yaml");
}

TEST(Configuration, configuration_config_files_unexistent)
{
	/* Test that including an unexistent file just skips it */
	const std::string main_conf_yaml =
		yaml_helper::configs_key + ":\n"
		"  - conf_5.yaml\n"
		"base_value:\n"
		"    id: 1\n"
		"    name: foo\n";

	std::ofstream outfile("main.yaml");
	outfile << main_conf_yaml;
	outfile.close();

	std::vector<std::string> cmdline_config_options;
	std::vector<std::string> loaded_conf_files;
	falco_configuration falco_config;
	ASSERT_NO_THROW(falco_config.init("main.yaml", loaded_conf_files, cmdline_config_options));

	// main
	ASSERT_EQ(loaded_conf_files.size(), 1);

	ASSERT_TRUE(falco_config.config.is_defined("base_value.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value.id", 0), 1);
	ASSERT_TRUE(falco_config.config.is_defined("base_value.name"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("base_value.name", ""), "foo");

	std::filesystem::remove("main.yaml");
}

TEST(Configuration, configuration_config_files_scalar_configs_files)
{
	/* Test that a single file can be included as a scalar (thanks to get_sequence_from_node magic) */
	const std::string main_conf_yaml =
		yaml_helper::configs_key + ": conf_2.yaml\n"
		"foo: bar\n"
		"base_value:\n"
		"    id: 1\n"
		"    name: foo\n";
	const std::string conf_yaml_2 =
		"foo2: bar2\n"
		"base_value_2:\n"
		"    id: 2\n";

	std::ofstream outfile("main.yaml");
	outfile << main_conf_yaml;
	outfile.close();

	outfile.open("conf_2.yaml");
	outfile << conf_yaml_2;
	outfile.close();

	std::vector<std::string> cmdline_config_options;
	std::vector<std::string> loaded_conf_files;
	falco_configuration falco_config;
	ASSERT_NO_THROW(falco_config.init("main.yaml", loaded_conf_files, cmdline_config_options));

	// main + conf_2
	ASSERT_EQ(loaded_conf_files.size(), 2);

	ASSERT_TRUE(falco_config.config.is_defined("foo"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("foo", ""), "bar");
	ASSERT_TRUE(falco_config.config.is_defined("base_value.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value.id", 0), 1);
	ASSERT_TRUE(falco_config.config.is_defined("base_value.name"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("base_value.name", ""), "foo");
	ASSERT_TRUE(falco_config.config.is_defined("foo2"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("foo2", ""), "bar2");
	ASSERT_TRUE(falco_config.config.is_defined("base_value_2.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value_2.id", 0), 2);

	std::filesystem::remove("main.yaml");
	std::filesystem::remove("conf_2.yaml");
}

TEST(Configuration, configuration_config_files_empty_configs_files)
{
	/* Test that empty includes list is accepted */
	const std::string main_conf_yaml =
		yaml_helper::configs_key + ":\n"
		"foo: bar\n"
		"base_value:\n"
		"    id: 1\n"
		"    name: foo\n";

	std::ofstream outfile("main.yaml");
	outfile << main_conf_yaml;
	outfile.close();

	std::vector<std::string> cmdline_config_options;
	std::vector<std::string> loaded_conf_files;
	falco_configuration falco_config;
	ASSERT_NO_THROW(falco_config.init("main.yaml", loaded_conf_files, cmdline_config_options));

	// main
	ASSERT_EQ(loaded_conf_files.size(), 1);

	ASSERT_TRUE(falco_config.config.is_defined("foo"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("foo", ""), "bar");
	ASSERT_TRUE(falco_config.config.is_defined("base_value.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value.id", 0), 1);
	ASSERT_TRUE(falco_config.config.is_defined("base_value.name"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("base_value.name", ""), "foo");

	std::filesystem::remove("main.yaml");
}

TEST(Configuration, configuration_config_files_self)
{
	/* Test that main config file cannot include itself */
	const std::string main_conf_yaml =
		yaml_helper::configs_key + ": main.yaml\n"
		"foo: bar\n"
		"base_value:\n"
		"    id: 1\n"
		"    name: foo\n";

	std::ofstream outfile("main.yaml");
	outfile << main_conf_yaml;
	outfile.close();

	std::vector<std::string> cmdline_config_options;
	std::vector<std::string> loaded_conf_files;
	falco_configuration falco_config;
	ASSERT_ANY_THROW(falco_config.init("main.yaml", loaded_conf_files, cmdline_config_options));

	std::filesystem::remove("main.yaml");
}

TEST(Configuration, configuration_config_files_directory)
{
	/*
	 * Test that when main config file includes a config directory,
	 * the config directory is parsed in lexicographic order,
	 * and only regular files are parsed.
	 */
	// Main config includes whole temp directory
	const std::string main_conf_yaml =
		yaml_helper::configs_key + ": " + std::filesystem::temp_directory_path().string() + "/test\n"
		   "foo: bar\n"
		   "base_value:\n"
		   "    id: 1\n"
		   "    name: foo\n";
	const std::string conf_yaml_2 =
		"foo2: bar2\n"
		"base_value_2:\n"
		"    id: 2\n";
	const std::string conf_yaml_3 =
		"foo2: bar3\n"
		"base_value_3:\n"
		"    id: 3\n"
		"    name: foo3\n";
	const std::string conf_yaml_4 =
		"foo4: bar4\n";

	std::filesystem::create_directory(std::filesystem::temp_directory_path() / "test");

	std::ofstream outfile("main.yaml");
	outfile << main_conf_yaml;
	outfile.close();

	outfile.open(std::filesystem::temp_directory_path()/"test/conf_2.yaml");
	outfile << conf_yaml_2;
	outfile.close();

	outfile.open(std::filesystem::temp_directory_path()/"test/conf_3.yaml");
	outfile << conf_yaml_3;
	outfile.close();

	// Create a directory and create a config inside it. We will later check that it was not parsed
	std::filesystem::create_directory(std::filesystem::temp_directory_path() / "test" / "foo");
	outfile.open(std::filesystem::temp_directory_path()/"test/foo/conf_4.yaml");
	outfile << conf_yaml_4;
	outfile.close();

	std::vector<std::string> cmdline_config_options;
	std::vector<std::string> loaded_conf_files;
	falco_configuration falco_config;
	ASSERT_NO_THROW(falco_config.init("main.yaml", loaded_conf_files, cmdline_config_options));

	// main + conf_2 + conf_3.
	// test/foo is not parsed.
	ASSERT_EQ(loaded_conf_files.size(), 3);

	ASSERT_TRUE(falco_config.config.is_defined("foo"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("foo", ""), "bar");
	ASSERT_TRUE(falco_config.config.is_defined("base_value.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value.id", 0), 1);
	ASSERT_TRUE(falco_config.config.is_defined("base_value.name"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("base_value.name", ""), "foo");
	ASSERT_TRUE(falco_config.config.is_defined("base_value_2"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value_2.id", 0), 2);
	ASSERT_TRUE(falco_config.config.is_defined("base_value_3.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value_3.id", 0), 3);
	ASSERT_TRUE(falco_config.config.is_defined("foo2"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("foo2", ""), "bar3");
	ASSERT_FALSE(falco_config.config.is_defined("foo4"));

	std::filesystem::remove("main");
	std::filesystem::remove_all(std::filesystem::temp_directory_path()/"test");
}

TEST(Configuration, configuration_config_files_cmdline)
{
	/* Test that we support including configs files from cmdline option */
	const std::string main_conf_yaml =
					   "foo: bar\n"
					   "base_value:\n"
					   "    id: 1\n"
					   "    name: foo\n";
	const std::string conf_yaml_2 =
		"foo2: bar2\n"
		"base_value_2:\n"
		"    id: 2\n";

	std::ofstream outfile("main.yaml");
	outfile << main_conf_yaml;
	outfile.close();

	outfile.open("conf_2.yaml");
	outfile << conf_yaml_2;
	outfile.close();

	// Pass "configs_files=..." cmdline option
	std::vector<std::string> cmdline_config_options;
	cmdline_config_options.push_back((yaml_helper::configs_key+"=conf_2.yaml"));

	std::vector<std::string> loaded_conf_files;
	falco_configuration falco_config;
	ASSERT_NO_THROW(falco_config.init("main.yaml", loaded_conf_files, cmdline_config_options));

	// main + conf_2
	ASSERT_EQ(loaded_conf_files.size(), 2);

	ASSERT_TRUE(falco_config.config.is_defined("foo"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("foo", ""), "bar");
	ASSERT_TRUE(falco_config.config.is_defined("base_value.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value.id", 0), 1);
	ASSERT_TRUE(falco_config.config.is_defined("base_value.name"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("base_value.name", ""), "foo");
	ASSERT_TRUE(falco_config.config.is_defined("foo2"));
	ASSERT_EQ(falco_config.config.get_scalar<std::string>("foo2", ""), "bar2");
	ASSERT_TRUE(falco_config.config.is_defined("base_value_2.id"));
	ASSERT_EQ(falco_config.config.get_scalar<int>("base_value_2.id", 0), 2);

	std::filesystem::remove("main.yaml");
	std::filesystem::remove("conf_2.yaml");
}

TEST(Configuration, configuration_environment_variables)
{
    // Set an environment variable for testing purposes
    std::string env_var_value = "envVarValue";
    std::string env_var_name = "ENV_VAR";
    SET_ENV_VAR(env_var_name.c_str(), env_var_value.c_str());

    std::string embedded_env_var_value = "${ENV_VAR}";
    std::string embedded_env_var_name = "ENV_VAR_EMBEDDED";
    SET_ENV_VAR(embedded_env_var_name.c_str(), embedded_env_var_value.c_str());

    std::string bool_env_var_value = "true";
    std::string bool_env_var_name = "ENV_VAR_BOOL";
    SET_ENV_VAR(bool_env_var_name.c_str(), bool_env_var_value.c_str());

    std::string int_env_var_value = "12";
    std::string int_env_var_name = "ENV_VAR_INT";
    SET_ENV_VAR(int_env_var_name.c_str(), int_env_var_value.c_str());

    std::string empty_env_var_value = "";
    std::string empty_env_var_name = "ENV_VAR_EMPTY";
    SET_ENV_VAR(empty_env_var_name.c_str(), empty_env_var_value.c_str());

    std::string default_value = "default";
    std::string env_var_sample_yaml =
        "base_value:\n"
        "    id: $ENV_VAR\n"
        "    name: '${ENV_VAR}'\n"
        "    string: my_string\n"
        "    invalid: $${ENV_VAR}\n"
	"    invalid_env: $$ENV_VAR\n"
	"    invalid_double_env: $${ENV_VAR}$${ENV_VAR}\n"
	"    invalid_embedded_env: $${${ENV_VAR}}\n"
	"    invalid_valid_env: $${ENV_VAR}${ENV_VAR}\n"
        "    escaped: \"${ENV_VAR}\"\n"
        "    subvalue:\n"
        "        subvalue2:\n"
        "            boolean: ${UNSED_XX_X_X_VAR}\n"
        "base_value_2:\n"
        "    sample_list:\n"
        "        - ${ENV_VAR}\n"
        "        - ' ${ENV_VAR}'\n"
	"        - '${ENV_VAR} '\n"
        "        - $UNSED_XX_X_X_VAR\n"
        "paths:\n"
	"    - ${ENV_VAR}/foo\n"
	"    - $ENV_VAR/foo\n"
	"    - /foo/${ENV_VAR}/\n"
	"    - /${ENV_VAR}/${ENV_VAR}${ENV_VAR}/foo\n"
	"    - ${ENV_VAR_EMBEDDED}/foo\n"
	"is_test: ${ENV_VAR_BOOL}\n"
	"num_test: ${ENV_VAR_INT}\n"
	"empty_test: ${ENV_VAR_EMPTY}\n"
	"plugins:\n"
	"  - name: k8saudit\n"
	"    library_path: /foo/${ENV_VAR}/libk8saudit.so\n"
	"    open_params: ${ENV_VAR_INT}\n";

    yaml_helper conf;
    conf.load_from_string(env_var_sample_yaml);

    /* Check if the base values are defined */
    ASSERT_TRUE(conf.is_defined("base_value"));
    ASSERT_TRUE(conf.is_defined("base_value_2"));
    ASSERT_TRUE(conf.is_defined("paths"));
    ASSERT_FALSE(conf.is_defined("unknown_base_value"));

    /* Test fetching of a regular string without any environment variable */
    auto base_value_string = conf.get_scalar<std::string>("base_value.string", default_value);
    ASSERT_EQ(base_value_string, "my_string");

    /* Test fetching of escaped environment variable format. Should return the string as-is after stripping the leading `$` */
    auto base_value_invalid = conf.get_scalar<std::string>("base_value.invalid", default_value);
    ASSERT_EQ(base_value_invalid, "${ENV_VAR}");

    /* Test fetching of invalid escaped environment variable format. Should return the string as-is */
    auto base_value_invalid_env = conf.get_scalar<std::string>("base_value.invalid_env", default_value);
    ASSERT_EQ(base_value_invalid_env, "$$ENV_VAR");

    /* Test fetching of 2 escaped environment variables side by side. Should return the string as-is after stripping the leading `$` */
    auto base_value_double_invalid = conf.get_scalar<std::string>("base_value.invalid_double_env", default_value);
    ASSERT_EQ(base_value_double_invalid, "${ENV_VAR}${ENV_VAR}");

    /*
     * Test fetching of escaped environment variable format with inside an env variable.
     * Should return the string as-is after stripping the leading `$` with the resolved env variable within
     */
    auto base_value_embedded_invalid = conf.get_scalar<std::string>("base_value.invalid_embedded_env", default_value);
    ASSERT_EQ(base_value_embedded_invalid, "${" + env_var_value + "}");

    /*
     * Test fetching of an escaped env variable plus an env variable side by side.
     * Should return the escaped one trimming the leading `$` plus the second one resolved.
     */
    auto base_value_valid_invalid = conf.get_scalar<std::string>("base_value.invalid_valid_env", default_value);
    ASSERT_EQ(base_value_valid_invalid, "${ENV_VAR}" + env_var_value);

    /* Test fetching of strings that contain environment variables */
    auto base_value_id = conf.get_scalar<std::string>("base_value.id", default_value);
    ASSERT_EQ(base_value_id, "$ENV_VAR"); // Does not follow the `${VAR}` format, so it should be treated as a regular string

    auto base_value_name = conf.get_scalar<std::string>("base_value.name", default_value);
    ASSERT_EQ(base_value_name, env_var_value); // Proper environment variable format

    auto base_value_escaped = conf.get_scalar<std::string>("base_value.escaped", default_value);
    ASSERT_EQ(base_value_escaped, env_var_value); // Environment variable within quotes

    /* Test fetching of an undefined environment variable. Resolves to empty string. */
    auto unknown_boolean = conf.get_scalar<std::string>("base_value.subvalue.subvalue2.boolean", default_value);
    ASSERT_EQ(unknown_boolean, "");

    /* Test fetching of environment variables from a list */
    auto base_value_2_list_0 = conf.get_scalar<std::string>("base_value_2.sample_list[0]", default_value);
    ASSERT_EQ(base_value_2_list_0, env_var_value); // Proper environment variable format

    auto base_value_2_list_1 = conf.get_scalar<std::string>("base_value_2.sample_list[1]", default_value);
    ASSERT_EQ(base_value_2_list_1, " " + env_var_value); // Environment variable preceded by a space, still extracted env var with leading space

    auto base_value_2_list_2 = conf.get_scalar<std::string>("base_value_2.sample_list[2]", default_value);
    ASSERT_EQ(base_value_2_list_2, env_var_value + " "); // Environment variable followed by a space, still extracted env var with trailing space

    auto base_value_2_list_3 = conf.get_scalar<std::string>("base_value_2.sample_list[3]", default_value);
    ASSERT_EQ(base_value_2_list_3, "$UNSED_XX_X_X_VAR"); // Does not follow the `${VAR}` format, so should be treated as a regular string

    /* Test expansion of environment variables within strings */
    auto path_list_0 = conf.get_scalar<std::string>("paths[0]", default_value);
    ASSERT_EQ(path_list_0, env_var_value + "/foo"); // Even if env var is part of bigger string, it gets expanded

    auto path_list_1 = conf.get_scalar<std::string>("paths[1]", default_value);
    ASSERT_EQ(path_list_1, "$ENV_VAR/foo"); // Does not follow the `${VAR}` format, so should be treated as a regular string

    auto path_list_2 = conf.get_scalar<std::string>("paths[2]", default_value);
    ASSERT_EQ(path_list_2, "/foo/" + env_var_value + "/"); // Even when env var is in the middle of a string. it gets expanded

    auto path_list_3 = conf.get_scalar<std::string>("paths[3]", default_value);
    ASSERT_EQ(path_list_3, "/" + env_var_value + "/" + env_var_value + env_var_value + "/foo"); // Even when the string contains multiple env vars they are correctly expanded

    auto path_list_4 = conf.get_scalar<std::string>("paths[4]", default_value);
    ASSERT_EQ(path_list_4, env_var_value + "/foo"); // Even when the env var contains another env var, it gets correctly double-expanded

    /* Check that variable expansion is type-aware */
    auto boolean = conf.get_scalar<bool>("is_test", false);
    ASSERT_EQ(boolean, true); // `true` can be parsed to bool.

    auto boolean_as_str = conf.get_scalar<std::string>("is_test", "false");
    ASSERT_EQ(boolean_as_str, "true"); // `true` can be parsed to string.

    auto boolean_as_int = conf.get_scalar<int32_t>("is_test", 0);
    ASSERT_EQ(boolean_as_int, 0); // `true` cannot be parsed to integer.

    auto integer = conf.get_scalar<int32_t>("num_test", -1);
    ASSERT_EQ(integer, 12);

    // An env var that resolves to an empty string returns ""
    auto empty_default_str = conf.get_scalar<std::string>("empty_test", default_value);
    ASSERT_EQ(empty_default_str, "");

    std::list<falco_configuration::plugin_config> plugins;
    conf.get_sequence<std::list<falco_configuration::plugin_config>>(plugins, std::string("plugins"));
    std::vector<falco_configuration::plugin_config> m_plugins{ std::make_move_iterator(std::begin(plugins)),
							      std::make_move_iterator(std::end(plugins)) };
    ASSERT_EQ(m_plugins[0].m_name, "k8saudit");
    ASSERT_EQ(m_plugins[0].m_library_path, "/foo/" + env_var_value + "/libk8saudit.so");
    ASSERT_EQ(m_plugins[0].m_open_params, "12");

    /* Clear the set environment variables after testing */
    SET_ENV_VAR(env_var_name.c_str(), "");
    SET_ENV_VAR(embedded_env_var_name.c_str(), "");
    SET_ENV_VAR(bool_env_var_name.c_str(), "");
    SET_ENV_VAR(int_env_var_name.c_str(), "");
    SET_ENV_VAR(empty_env_var_name.c_str(), "");
}

TEST(Configuration, configuration_webserver_ip)
{
    falco_configuration falco_config;

    std::vector<std::string> valid_addresses = {"127.0.0.1",
                                                "1.127.0.1",
                                                "1.1.127.1",
                                                "1.1.1.127",
                                                "::",
                                                "::1",
                                                "1200:0000:AB00:1234:0000:2552:7777:1313",
                                                "1200::AB00:1234:0000:2552:7777:1313",
                                                "1200:0000:AB00:1234::2552:7777:1313",
                                                "21DA:D3:0:2F3B:2AA:FF:FE28:9C5A",
                                                "FE80:0000:0000:0000:0202:B3FF:FE1E:8329",
                                                "0.0.0.0",
                                                "9.255.255.255",
                                                "11.0.0.0",
                                                "126.255.255.255",
                                                "129.0.0.0",
                                                "169.253.255.255",
                                                "169.255.0.0",
                                                "172.15.255.255",
                                                "172.32.0.0",
                                                "191.0.1.255",
                                                "192.88.98.255",
                                                "192.88.100.0",
                                                "192.167.255.255",
                                                "192.169.0.0",
                                                "198.17.255.255",
                                                "223.255.255.255"};

    for (const std::string &address: valid_addresses) {
        std::string option = "webserver.listen_address=";
        option.append(address);

        std::vector<std::string> cmdline_config_options;
        cmdline_config_options.push_back(option);

        EXPECT_NO_THROW(falco_config.init(cmdline_config_options));

        ASSERT_EQ(falco_config.m_webserver_listen_address, address);
    }

    std::vector<std::string> invalid_addresses = {"327.0.0.1",
                                                  "1.327.0.1",
                                                  "1.1.327.1",
                                                  "1.1.1.327",
                                                  "12 7.0.0.1",
                                                  "127. 0.0.1",
                                                  "127.0. 0.1",
                                                  "127.0.0. 1",
                                                  "!27.0.0.1",
                                                  "1200: 0000:AB00:1234:0000:2552:7777:1313",
                                                  "1200:0000: AB00:1234:0000:2552:7777:1313",
                                                  "1200:0000:AB00: 1234:0000:2552:7777:1313",
                                                  "1200:0000:AB00:1234: 0000:2552:7777:1313",
                                                  "1200:0000:AB00:1234:0000: 2552:7777:1313",
                                                  "1200:0000:AB00:1234:0000:2552: 7777:1313",
                                                  "1200:0000:AB00:1234:0000:2552:7777: 1313",
                                                  "1200:0000:AB00:1234:0000:2552:7777:131G",
                                                  "1200:0000:AB00:1234:0000:2552:77Z7:1313",
                                                  "1200:0000:AB00:1234:0000:2G52:7777:1313",
                                                  "1200:0000:AB00:1234:0O00:2552:7777:1313",
                                                  "1200:0000:AB00:H234:0000:2552:7777:1313",
                                                  "1200:0000:IB00:1234:0000:2552:7777:1313",
                                                  "1200:0O00:AB00:1234:0000:2552:7777:1313",
                                                  "12O0:0000:AB00:1234:0000:2552:7777:1313",};

    for (const std::string &address: invalid_addresses) {
        std::string option = "webserver.listen_address=";
        option.append(address);

        std::vector<std::string> cmdline_config_options;
        cmdline_config_options.push_back(option);

        EXPECT_ANY_THROW(falco_config.init(cmdline_config_options));
    }
}
