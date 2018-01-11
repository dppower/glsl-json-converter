#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <regex>
#include <boost/program_options.hpp>

#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31

// Patterns for parsing glsl:
const std::regex include_header("^#include <glsl-([0-9]{3})-?(es)?\\.h>$");
const std::regex version_header("^#version ([0-9]{3})(?: es)?$");
const std::regex trim_comments_spaces("^\\s*(?:(?:(.*?)\\s*[/]{2,}.*)|(?:(.*?)\\s*))$");
const std::regex validate_line("^(?:(?:[a-z][a-zA-Z0-9\\s_+*\\-/=><&|^?:{().,;[\\]]*[;{})])|[}])$");
// Files should have the extensions: .vs or .vs.glsl or .fs or .fs.glsl
const std::regex input_file_name_pattern("^.*\\\\(.*)\\.(vs|fs)(?:\\.glsl)?$");

std::string trim_line(std::string current_line, unsigned int line_count) {

	std::string trimmed_line = "";

	if (line_count == 1) {		
		std::smatch sub_matches;		
		if (std::regex_match(current_line, sub_matches, include_header)) {
			trimmed_line = "#version " + sub_matches[1].str();
			if (sub_matches[2].str() != "") {
				trimmed_line += " " + sub_matches[2].str();
			}
		}
		else {
			if (std::regex_match(current_line, sub_matches, version_header)) {
				trimmed_line = current_line;
			}
			else {
				std::cout << "Syntax error(1): unrecognised glsl version, " << current_line << "\n";
			}
		}
	}
	else {
		
		std::smatch sub_matches;

		if (std::regex_match(current_line, sub_matches, trim_comments_spaces)) {
			std::string sub_string = sub_matches[1].str() + sub_matches[2].str();

			if (sub_string != "") {

				if (std::regex_match(sub_string.begin(), sub_string.end(), validate_line)) {
					trimmed_line = sub_string;
				}
				else {
					std::cout << "Syntax error(3): " << line_count << ": " << sub_string << "\n";
				}
			}
		}
		else {
			std::cout << "Syntax error(2): " << line_count << ": " << current_line << "\n";
		}
	}

	return trimmed_line;
}

void find_variable_name(std::vector<std::string>& variable_array, std::string trimmed_line, std::string qualifier_name) {

	std::regex qualifier("^.*" + qualifier_name + "\\s+\\w+\\s+(\\w+);");

	std::smatch sub_matches;

	if (std::regex_match(trimmed_line, sub_matches, qualifier)) {
		std::string variable_name = sub_matches[1].str();
		variable_array.push_back(variable_name);
	}	
}

std::string variable_array_to_string(const std::vector<std::string>& variable_array, std::string variable_name, std::string delimiter = "") {
	std::string array_string = delimiter + "\"" + variable_name + "\": [";
	for (auto i : variable_array) {
		array_string += "\"" + i + "\", ";
	}
	size_t last_comma = array_string.find_last_of(",");
	if (last_comma != std::string::npos && !variable_array.empty()) {
		array_string.pop_back();
		array_string.pop_back();
	}
	array_string += "],\n";

	return array_string;
}

std::string parse_file(std::string file_name) {
	std::ifstream ifs(file_name);
	if (ifs.is_open()) {		
		std::smatch sub_matches;

		if (std::regex_match(file_name, sub_matches, input_file_name_pattern)) {
			std::string type = sub_matches[2] != "" ? sub_matches[2].str() : sub_matches[3].str();
			int gl_type = (type == "vs") ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
			std::string shader_name = sub_matches[1].str();
			std::string shader_id = shader_name + "." + type;

			std::vector<std::string> attributes = {};
			std::vector<std::string> uniforms = {};
			std::string source = "\"source\": \"";

			std::string current_line;
			unsigned int line_count = 1;
			while (std::getline(ifs, current_line)) {
				std::string trimmed_line = trim_line(current_line, line_count);

				if (trimmed_line != "") {

					find_variable_name(attributes, trimmed_line, "attribute");
					find_variable_name(attributes, trimmed_line, "in");
					find_variable_name(uniforms, trimmed_line, "uniform");

					source += trimmed_line + "\\n";
				}
				line_count++;
			}

			std::string attribute_string = variable_array_to_string(attributes, "attributes");
			std::string uniform_string = variable_array_to_string(uniforms, "uniforms");

			ifs.close();
			return "\t\"" + shader_id + "\": {\n\t\t\"name\":\"" + shader_name + "\",\n\t\t\"type\":" +
				std::to_string(gl_type) + ",\n\t\t" + attribute_string + "\t\t" + uniform_string +
				"\t\t" + source + "\"\n\t}";
		}
		else {
			std::cout << "Unrecognised file type: " << file_name << "\n";			
		}
	}
	else {
		std::cout << "Could not open file: " << file_name << "\n";
	}
	return "";
}

int main(int argc, char** argv) {
	std::cout << "Running..." << "\n";
	namespace bpo = boost::program_options;

	std::vector<std::string> input_file_list;
	std::string target_directory;

	bpo::options_description desc("");
	desc.add_options()
		("input,i", bpo::value<std::vector<std::string>>(&input_file_list)->multitoken(), "A list of files to use as input.")
		("output,o", bpo::value<std::string>(&target_directory), "The directory for output.");

	bpo::variables_map arg_map;
	try {
		bpo::store(bpo::parse_command_line(argc, argv, desc), arg_map);
		bpo::notify(arg_map);
	}
	catch (bpo::error& err) {
		std::cout << "Error: " << err.what() << "\n";
		return 1;
	}

	for (auto name : input_file_list) {
		std::cout << name << "\n";
	}

	std::cout << target_directory << "\n";
	std::string output_file = target_directory + "shader-source.json";

	std::ofstream ofs(output_file);
	if (ofs.is_open()) {
		ofs << "{\n";
		size_t file_count = input_file_list.size();
		for (size_t i = 0; i < file_count; i++) {
			std::string file_name = input_file_list[i];
			std::string ending = (i < file_count - 1) ? ",\n" : "\n";
			ofs << parse_file(file_name) << ending;
		}
		ofs << "}";
		ofs.close();
	}
	else {
		std::cout << "Could not open the output file: " << output_file << ".\n";
	}
	return 0;
}