// Copyright 2015 The Weave Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/commands/command_dictionary.h"

#include <gtest/gtest.h>

#include "src/commands/unittest_utils.h"

namespace weave {

using test::CreateDictionaryValue;
using test::IsEqualValue;

TEST(CommandDictionary, Empty) {
  CommandDictionary dict;
  EXPECT_TRUE(dict.IsEmpty());
  EXPECT_EQ(nullptr, dict.FindCommand("robot.jump"));
}

TEST(CommandDictionary, LoadCommands) {
  auto json = CreateDictionaryValue(R"({
    'robot': {
      'jump': {
        'parameters': {
          'height': 'integer',
          '_jumpType': ['_withAirFlip', '_withSpin', '_withKick']
        },
        'progress': {
          'progress': 'integer'
        },
        'results': {}
      }
    }
  })");
  CommandDictionary dict;
  EXPECT_TRUE(dict.LoadCommands(*json, nullptr, nullptr));
  EXPECT_EQ(1, dict.GetSize());
  EXPECT_NE(nullptr, dict.FindCommand("robot.jump"));
  json = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': 'integer'}
      },
      'shutdown': {
      }
    }
  })");
  EXPECT_TRUE(dict.LoadCommands(*json, nullptr, nullptr));
  EXPECT_EQ(3, dict.GetSize());
  EXPECT_NE(nullptr, dict.FindCommand("robot.jump"));
  EXPECT_NE(nullptr, dict.FindCommand("base.reboot"));
  EXPECT_NE(nullptr, dict.FindCommand("base.shutdown"));
  EXPECT_EQ(nullptr, dict.FindCommand("foo.bar"));
}

TEST(CommandDictionary, LoadWithInheritance) {
  auto json = CreateDictionaryValue(R"({
    'robot': {
      'jump': {
        'minimalRole': 'viewer',
        'visibility':'local',
        'parameters': {
          'height': 'integer'
        },
        'progress': {
          'progress': 'integer'
        },
        'results': {
          'success': 'boolean'
        }
      }
    }
  })");
  CommandDictionary base_dict;
  EXPECT_TRUE(base_dict.LoadCommands(*json, nullptr, nullptr));
  EXPECT_EQ(1, base_dict.GetSize());
  json = CreateDictionaryValue(R"({'robot': {'jump': {}}})");

  CommandDictionary dict;
  EXPECT_TRUE(dict.LoadCommands(*json, &base_dict, nullptr));
  EXPECT_EQ(1, dict.GetSize());

  auto cmd = dict.FindCommand("robot.jump");
  EXPECT_NE(nullptr, cmd);

  EXPECT_EQ("local", cmd->GetVisibility().ToString());
  EXPECT_EQ(UserRole::kViewer, cmd->GetMinimalRole());

  EXPECT_JSON_EQ("{'height': {'type': 'integer'}}",
                 *cmd->GetParameters()->ToJson(true, true));
  EXPECT_JSON_EQ("{'progress': {'type': 'integer'}}",
                 *cmd->GetProgress()->ToJson(true, false));
  EXPECT_JSON_EQ("{'success': {'type': 'boolean'}}",
                 *cmd->GetResults()->ToJson(true, false));
}

TEST(CommandDictionary, LoadCommands_Failures) {
  CommandDictionary dict;
  ErrorPtr error;

  // Command definition is not an object.
  auto json = CreateDictionaryValue("{'robot':{'jump':0}}");
  EXPECT_FALSE(dict.LoadCommands(*json, nullptr, &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
  error.reset();

  // Package definition is not an object.
  json = CreateDictionaryValue("{'robot':'blah'}");
  EXPECT_FALSE(dict.LoadCommands(*json, nullptr, &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
  error.reset();

  // Invalid command definition is not an object.
  json = CreateDictionaryValue(
      "{'robot':{'jump':{'parameters':{'flip':0},'results':{}}}}");
  EXPECT_FALSE(dict.LoadCommands(*json, nullptr, &error));
  EXPECT_EQ("invalid_object_schema", error->GetCode());
  EXPECT_NE(nullptr, error->GetInnerError());  // Must have additional info.
  error.reset();

  // Empty command name.
  json = CreateDictionaryValue("{'robot':{'':{'parameters':{},'results':{}}}}");
  EXPECT_FALSE(dict.LoadCommands(*json, nullptr, &error));
  EXPECT_EQ("invalid_command_name", error->GetCode());
  error.reset();
}

TEST(CommandDictionaryDeathTest, LoadCommands_RedefineInDifferentCategory) {
  // Redefine commands in different category.
  CommandDictionary dict;
  ErrorPtr error;
  auto json = CreateDictionaryValue("{'robot':{'jump':{}}}");
  dict.LoadCommands(*json, nullptr, &error);
  ASSERT_DEATH(dict.LoadCommands(*json, nullptr, &error),
               ".*Definition for command 'robot.jump' overrides an "
               "earlier definition");
}

TEST(CommandDictionary, LoadCommands_CustomCommandNaming) {
  // Custom command must start with '_'.
  CommandDictionary base_dict;
  CommandDictionary dict;
  ErrorPtr error;
  auto json = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': 'integer'},
        'results': {}
      }
    }
  })");
  base_dict.LoadCommands(*json, nullptr, &error);
  EXPECT_TRUE(dict.LoadCommands(*json, &base_dict, &error));
  auto json2 =
      CreateDictionaryValue("{'base':{'jump':{'parameters':{},'results':{}}}}");
  EXPECT_FALSE(dict.LoadCommands(*json2, &base_dict, &error));
  EXPECT_EQ("invalid_command_name", error->GetCode());
  error.reset();

  // If the command starts with "_", then it's Ok.
  json2 = CreateDictionaryValue(
      "{'base':{'_jump':{'parameters':{},'results':{}}}}");
  EXPECT_TRUE(dict.LoadCommands(*json2, &base_dict, nullptr));
}

TEST(CommandDictionary, LoadCommands_RedefineStdCommand) {
  // Redefine commands parameter type.
  CommandDictionary base_dict;
  CommandDictionary dict;
  ErrorPtr error;
  auto json = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': 'integer'},
        'results': {'version': 'integer'}
      }
    }
  })");
  base_dict.LoadCommands(*json, nullptr, &error);

  auto json2 = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': 'string'},
        'results': {'version': 'integer'}
      }
    }
  })");
  EXPECT_FALSE(dict.LoadCommands(*json2, &base_dict, &error));
  EXPECT_EQ("invalid_object_schema", error->GetCode());
  EXPECT_EQ("invalid_parameter_definition", error->GetInnerError()->GetCode());
  EXPECT_EQ("param_type_changed", error->GetFirstError()->GetCode());
  error.reset();

  auto json3 = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': 'integer'},
        'results': {'version': 'string'}
      }
    }
  })");
  EXPECT_FALSE(dict.LoadCommands(*json3, &base_dict, &error));
  EXPECT_EQ("invalid_object_schema", error->GetCode());
  // TODO(antonm): remove parameter from error below and use some generic.
  EXPECT_EQ("invalid_parameter_definition", error->GetInnerError()->GetCode());
  EXPECT_EQ("param_type_changed", error->GetFirstError()->GetCode());
  error.reset();
}

TEST(CommandDictionary, GetCommandsAsJson) {
  auto json_base = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': {'maximum': 100}},
        'results': {}
      },
      'shutdown': {
        'parameters': {},
        'results': {}
      }
    }
  })");
  CommandDictionary base_dict;
  base_dict.LoadCommands(*json_base, nullptr, nullptr);

  auto json = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': {'minimum': 10}},
        'results': {}
      }
    },
    'robot': {
      '_jump': {
        'parameters': {'_height': 'integer'},
        'results': {}
      }
    }
  })");
  CommandDictionary dict;
  dict.LoadCommands(*json, &base_dict, nullptr);

  json = dict.GetCommandsAsJson(
      [](const CommandDefinition* def) { return true; }, false, nullptr);
  ASSERT_NE(nullptr, json.get());
  auto expected = R"({
    'base': {
      'reboot': {
        'parameters': {'delay': {'minimum': 10}},
        'minimalRole': 'user'
      }
    },
    'robot': {
      '_jump': {
        'parameters': {'_height': 'integer'},
        'minimalRole': 'user'
      }
    }
  })";
  EXPECT_JSON_EQ(expected, *json);

  json = dict.GetCommandsAsJson(
      [](const CommandDefinition* def) { return true; }, true, nullptr);
  ASSERT_NE(nullptr, json.get());
  expected = R"({
    'base': {
      'reboot': {
        'parameters': {
          'delay': {
            'maximum': 100,
            'minimum': 10,
            'type': 'integer'
          }
        },
        'minimalRole': 'user'
      }
    },
    'robot': {
      '_jump': {
        'parameters': {
          '_height': {
           'type': 'integer'
          }
        },
        'minimalRole': 'user'
      }
    }
  })";
  EXPECT_JSON_EQ(expected, *json);
}

TEST(CommandDictionary, GetCommandsAsJsonWithVisibility) {
  auto json = CreateDictionaryValue(R"({
    'test': {
      'command1': {
        'parameters': {},
        'results': {},
        'visibility': 'none'
      },
      'command2': {
        'parameters': {},
        'results': {},
        'visibility': 'local'
      },
      'command3': {
        'parameters': {},
        'results': {},
        'visibility': 'cloud'
      },
      'command4': {
        'parameters': {},
        'results': {},
        'visibility': 'all'
      },
      'command5': {
        'parameters': {},
        'results': {},
        'visibility': 'none'
      },
      'command6': {
        'parameters': {},
        'results': {},
        'visibility': 'local'
      },
      'command7': {
        'parameters': {},
        'results': {},
        'visibility': 'cloud'
      },
      'command8': {
        'parameters': {},
        'results': {},
        'visibility': 'all'
      }
    }
  })");
  CommandDictionary dict;
  ASSERT_TRUE(dict.LoadCommands(*json, nullptr, nullptr));

  json = dict.GetCommandsAsJson(
      [](const CommandDefinition* def) { return true; }, false, nullptr);
  ASSERT_NE(nullptr, json.get());
  auto expected = R"({
    'test': {
      'command1': {'parameters': {}, 'minimalRole': 'user'},
      'command2': {'parameters': {}, 'minimalRole': 'user'},
      'command3': {'parameters': {}, 'minimalRole': 'user'},
      'command4': {'parameters': {}, 'minimalRole': 'user'},
      'command5': {'parameters': {}, 'minimalRole': 'user'},
      'command6': {'parameters': {}, 'minimalRole': 'user'},
      'command7': {'parameters': {}, 'minimalRole': 'user'},
      'command8': {'parameters': {}, 'minimalRole': 'user'}
    }
  })";
  EXPECT_JSON_EQ(expected, *json);

  json = dict.GetCommandsAsJson(
      [](const CommandDefinition* def) { return def->GetVisibility().local; },
      false, nullptr);
  ASSERT_NE(nullptr, json.get());
  expected = R"({
    'test': {
      'command2': {'parameters': {}, 'minimalRole': 'user'},
      'command4': {'parameters': {}, 'minimalRole': 'user'},
      'command6': {'parameters': {}, 'minimalRole': 'user'},
      'command8': {'parameters': {}, 'minimalRole': 'user'}
    }
  })";
  EXPECT_JSON_EQ(expected, *json);

  json = dict.GetCommandsAsJson(
      [](const CommandDefinition* def) { return def->GetVisibility().cloud; },
      false, nullptr);
  ASSERT_NE(nullptr, json.get());
  expected = R"({
    'test': {
      'command3': {'parameters': {}, 'minimalRole': 'user'},
      'command4': {'parameters': {}, 'minimalRole': 'user'},
      'command7': {'parameters': {}, 'minimalRole': 'user'},
      'command8': {'parameters': {}, 'minimalRole': 'user'}
    }
  })";
  EXPECT_JSON_EQ(expected, *json);

  json = dict.GetCommandsAsJson(
      [](const CommandDefinition* def) {
        return def->GetVisibility().local && def->GetVisibility().cloud;
      },
      false, nullptr);
  ASSERT_NE(nullptr, json.get());
  expected = R"({
    'test': {
      'command4': {'parameters': {}, 'minimalRole': 'user'},
      'command8': {'parameters': {}, 'minimalRole': 'user'}
    }
  })";
  EXPECT_JSON_EQ(expected, *json);
}

TEST(CommandDictionary, LoadWithPermissions) {
  CommandDictionary base_dict;
  auto json = CreateDictionaryValue(R"({
    'base': {
      'command1': {
        'parameters': {},
        'results': {},
        'visibility':'none'
      },
      'command2': {
        'minimalRole': 'viewer',
        'parameters': {},
        'results': {},
        'visibility':'local'
      },
      'command3': {
        'minimalRole': 'user',
        'parameters': {},
        'results': {},
        'visibility':'cloud'
      },
      'command4': {
        'minimalRole': 'manager',
        'parameters': {},
        'results': {},
        'visibility':'all'
      },
      'command5': {
        'minimalRole': 'owner',
        'parameters': {},
        'results': {},
        'visibility':'local,cloud'
      }
    }
  })");
  EXPECT_TRUE(base_dict.LoadCommands(*json, nullptr, nullptr));

  auto cmd = base_dict.FindCommand("base.command1");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("none", cmd->GetVisibility().ToString());
  EXPECT_EQ(UserRole::kUser, cmd->GetMinimalRole());

  cmd = base_dict.FindCommand("base.command2");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("local", cmd->GetVisibility().ToString());
  EXPECT_EQ(UserRole::kViewer, cmd->GetMinimalRole());

  cmd = base_dict.FindCommand("base.command3");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("cloud", cmd->GetVisibility().ToString());
  EXPECT_EQ(UserRole::kUser, cmd->GetMinimalRole());

  cmd = base_dict.FindCommand("base.command4");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("all", cmd->GetVisibility().ToString());
  EXPECT_EQ(UserRole::kManager, cmd->GetMinimalRole());

  cmd = base_dict.FindCommand("base.command5");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("all", cmd->GetVisibility().ToString());
  EXPECT_EQ(UserRole::kOwner, cmd->GetMinimalRole());

  CommandDictionary dict;
  json = CreateDictionaryValue(R"({
    'base': {
      'command1': {
        'parameters': {},
        'results': {}
      },
      'command2': {
        'parameters': {},
        'results': {}
      },
      'command3': {
        'parameters': {},
        'results': {}
      },
      'command4': {
        'parameters': {},
        'results': {}
      },
      'command5': {
        'parameters': {},
        'results': {}
      },
      '_command6': {
        'parameters': {},
        'results': {}
      }
    }
  })");
  EXPECT_TRUE(dict.LoadCommands(*json, &base_dict, nullptr));

  cmd = dict.FindCommand("base.command1");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("none", cmd->GetVisibility().ToString());
  EXPECT_EQ(UserRole::kUser, cmd->GetMinimalRole());

  cmd = dict.FindCommand("base.command2");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("local", cmd->GetVisibility().ToString());
  EXPECT_EQ(UserRole::kViewer, cmd->GetMinimalRole());

  cmd = dict.FindCommand("base.command3");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("cloud", cmd->GetVisibility().ToString());
  EXPECT_EQ(UserRole::kUser, cmd->GetMinimalRole());

  cmd = dict.FindCommand("base.command4");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("all", cmd->GetVisibility().ToString());
  EXPECT_EQ(UserRole::kManager, cmd->GetMinimalRole());

  cmd = dict.FindCommand("base.command5");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("all", cmd->GetVisibility().ToString());
  EXPECT_EQ(UserRole::kOwner, cmd->GetMinimalRole());

  cmd = dict.FindCommand("base._command6");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("all", cmd->GetVisibility().ToString());
  EXPECT_EQ(UserRole::kUser, cmd->GetMinimalRole());
}

TEST(CommandDictionary, LoadWithPermissions_InvalidVisibility) {
  CommandDictionary dict;
  ErrorPtr error;

  auto json = CreateDictionaryValue(R"({
    'base': {
      'jump': {
        'parameters': {},
        'results': {},
        'visibility':'foo'
      }
    }
  })");
  EXPECT_FALSE(dict.LoadCommands(*json, nullptr, &error));
  EXPECT_EQ("invalid_command_visibility", error->GetCode());
  EXPECT_EQ("invalid_parameter_value", error->GetInnerError()->GetCode());
  error.reset();
}

TEST(CommandDictionary, LoadWithPermissions_InvalidRole) {
  CommandDictionary dict;
  ErrorPtr error;

  auto json = CreateDictionaryValue(R"({
    'base': {
      'jump': {
        'parameters': {},
        'results': {},
        'visibility':'local,cloud',
        'minimalRole':'foo'
      }
    }
  })");
  EXPECT_FALSE(dict.LoadCommands(*json, nullptr, &error));
  EXPECT_EQ("invalid_minimal_role", error->GetCode());
  EXPECT_EQ("invalid_parameter_value", error->GetInnerError()->GetCode());
  error.reset();
}

}  // namespace weave