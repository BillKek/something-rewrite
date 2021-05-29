#include "aids.hpp"
using namespace aids;

#include "./config_def.hpp"
#include "./config_parser.hpp"

void print1(FILE *stream, Config_Type type)
{
    switch (type) {
    case Config_Type::Float:
        print(stream, "Config_Type::Float");
        break;
    case Config_Type::Color:
        print(stream, "Config_Type::Color");
        break;
    case Config_Type::Int:
        print(stream, "Config_Type::Int");
        break;
    case Config_Type::Count:
    default:
        unreachable("print1(Config_Type)");
    }
}

void print1(FILE *stream, Config_Value_Def def)
{
    print(stream, "{\"", def.name, "\"_sv, ", def.type,"}");
}

Maybe<size_t> where_var_is_defined(const Dynamic_Array<Config_Value_Def> &defs,
                                   const Dynamic_Array<size_t> &def_lines,
                                   String_View var_name)
{
    for (size_t i = 0; i < defs.size; ++i) {
        if (defs[i].name == var_name) {
            return {true, def_lines[i]};
        }
    }
    return {};
}

void usage(const char *program, FILE *stream)
{
    println(stream, "Usage: ", program, " [--bake] <vars.conf>");
}

int main(int argc, char **argv)
{
    Args args = {argc, argv};
    const auto program = args.shift();

    const char *vars_conf_path = NULL;
    bool bake = false;

    while (!args.empty()) {
        const char *flag = args.shift();
        if (strcmp(flag, "--bake") == 0) {
            bake = true;
        } else {
            if (vars_conf_path != NULL) {
                usage(program, stderr);
                panic("ERROR: only one path to vars.conf can be provided");
            }

            vars_conf_path = flag;
        }
    }

    if (vars_conf_path == NULL) {
        usage(program, stderr);
        panic("ERROR: path to vars.conf was not provided");
    }

    const String_View vars_conf_content =
        unwrap_or_panic(
            read_file_as_string_view(vars_conf_path),
            "Could not read file `", vars_conf_path, "`: ", strerror(errno));
    defer(destroy(vars_conf_content));

    Dynamic_Array<Config_Value_Def> config_value_defs = {};
    defer(destroy(config_value_defs));

    Dynamic_Array<size_t> config_value_def_lines = {};
    defer(destroy(config_value_def_lines));

    Dynamic_Array<Config_Value> config_values = {};
    defer(destroy(config_values));

    Parser parser = {};
    parser.content = vars_conf_content;

    while (parser.next() == Parser::SUCCESS) {
        auto place = where_var_is_defined(
                         config_value_defs,
                         config_value_def_lines,
                         parser.def.name);

        if (place.has_value) {
            panic(vars_conf_path, ":", parser.line_number, ": `", parser.def.name, "` is already defined", Newline(),
                  vars_conf_path, ":", place.unwrap, ": it was defined first time here");
        }

        config_values.push(parser.value);
        config_value_defs.push(parser.def);
        config_value_def_lines.push(parser.line_number);
    }

    switch (parser.status) {
    case Parser::INVALID_TYPE:
        panic(vars_conf_path, ":", parser.line_number, ": `", parser.invalid_type.name, "` is not a valid type");
    case Parser::INVALID_VALUE:
        panic(vars_conf_path, ":", parser.line_number, ": could not parse `", parser.invalid_value.value, "` as `", config_type_name(parser.invalid_value.expected_type), "`");
    case Parser::SUCCESS:
        unreachable("Parser::SUCCESS");
    case Parser::FINISHED:
    {}
    }

    println(stdout, "// Generated by ", __FILE__, " from ", vars_conf_path);
    println(stdout, "#ifndef CONFIG_INDEX_HPP_");
    println(stdout, "#define CONFIG_INDEX_HPP_");
    println(stdout);
    if (bake) {
        for (size_t i = 0; i < config_value_defs.size; ++i) {
            switch (config_value_defs[i].type) {
            case Config_Type::Float:
                println(stdout, "const float ", config_value_defs[i].name, " = ", config_values[i].as_float, ";");
                break;
            case Config_Type::Color:
                println(stdout, "const RGBA ", config_value_defs[i].name, " = ", config_values[i].as_color, ";");
                break;
            case Config_Type::Int:
                println(stdout, "const int ", config_value_defs[i].name, " = ", config_values[i].as_int, ";");
                break;
            case Config_Type::Count:
            default:
                unreachable("baked config_values macros");
            }
        }
        println(stdout);
    } else {
        println(stdout, "#include \"./config_def.hpp\"");
        println(stdout);
        println(stdout, "const size_t CONFIG_CAPACITY = ", config_value_defs.size, ";");
        println(stdout, "const Config_Value_Def config_value_defs[CONFIG_CAPACITY] = {");
        for (size_t i = 0; i < config_value_defs.size; ++i) {
            println(stdout, "   {\"", config_value_defs[i].name, "\"_sv, ", config_value_defs[i].type,"},");
        }
        println(stdout, "};");
        println(stdout);
        println(stdout, "Config_Value config_values[CONFIG_CAPACITY] = {};");
        println(stdout);
        for (size_t i = 0; i < config_value_defs.size; ++i) {
            println(stdout, "#define ", config_value_defs[i].name,
                    " config_values[", i, "].as_", config_type_name(config_value_defs[i].type));
        }
        println(stdout);
    }
    println(stdout, "#endif // CONFIG_INDEX_HPP_");

    return 0;
}

#define AIDS_IMPLEMENTATION
#include "aids.hpp"
