mod tokenizer;
mod parser;

use std::fs;

const FILENAME: &str = "tests/in/test.kong";

fn main() {
	let contents = fs::read_to_string(FILENAME).expect(&format!("Could not read file {}.", FILENAME));

	let tokens = tokenizer::tokenize(&contents);
	for token in &tokens {
		println!("{:?}", token);
	}
	let _parse_tree = parser::parse(tokens);
}
