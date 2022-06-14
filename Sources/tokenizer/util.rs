use super::token::Token;

pub fn is_num(ch: char) -> bool {
	ch == '0' || ch == '1' || ch == '2' || ch == '3' || ch == '4' || ch == '5' || ch == '6' || ch == '7' || ch == '8' || ch == '9'
}

pub fn is_op(ch: char) -> bool {
	ch == '&' || ch == '|' || ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '=' || ch == '!' || ch == '<' || ch == '>' || ch == '%'
}

pub fn is_whitespace(ch: char) -> bool {
	ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t'
}

pub fn push_identifier(tokens: &mut Vec<Token>, buffer: &str) {
	match buffer {
		"true" => tokens.push(Token::Boolean(true)),
		"false" => tokens.push(Token::Boolean(false)),
		"if" => tokens.push(Token::If),
		"float" => tokens.push(Token::Float),
		"in" => tokens.push(Token::In),
		"vec3" => tokens.push(Token::Vec3),
		"vec4" => tokens.push(Token::Vec4),
		"void" => tokens.push(Token::Void),
		"struct" => tokens.push(Token::Struct),
		"fn" => tokens.push(Token::Function),
		"let" => tokens.push(Token::Let),
		"mut" => tokens.push(Token::Mut),
		_ => tokens.push(Token::Identifier(buffer.to_string())),
	}
}

pub fn push_attribute(tokens: &mut Vec<Token>, buffer: &str) {
	tokens.push(Token::Attribute(buffer.to_string()));
}

pub enum Mode {
	Select,
	Number,
	Stringy,
	Operator,
	Identifier,
	LineComment,
	Comment,
	Attribute,
}

pub struct State<'a> {
	iterator: &'a mut std::str::Chars<'a>,
	pub next: Option<char>,
	pub next_next: Option<char>,
}

impl<'a> State<'a> {
	pub fn new(it: &'a mut std::str::Chars<'a>) -> State<'a> {
		let next = it.next();
		let next_next = it.next();
		State {iterator: it, next, next_next}
	}

	pub fn advance(&mut self) {
		self.next = self.next_next;
		self.next_next = self.iterator.next();
	}
}

pub fn parse_number(buffer: &str) -> f64 {
	return buffer.parse::<f64>().expect(&format!("Failed to parse \"{}\" as a number.", buffer));
}
