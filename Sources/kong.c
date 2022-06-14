const char *filename = "tests/in/test.kong";

int main(int argc, char **argv) {
	const char *contents = fs::read_to_string(FILENAME).expect(&format !("Could not read file {}.", FILENAME));

	tokens_t tokens = tokenizer::tokenize(&contents);
	// for token in &tokens {
	//	println !("{:?}", token);
	// }
	tree_t _parse_tree = parser_parse(tokens);

	return 0;
}
