use super::statement::*;
use super::super::tokenizer::token::*;

pub enum Expression {
	Binary(ExprBinary),
	Unary(ExprUnary),
	Boolean(bool),
	Number(f64),
	Stringy(String),
	Variable(String),
	Grouping(Box<Expression>),
	Call(CallStatement),
	Member(MemberExpression),
	Constructor(ConstructorExpression),
}

pub struct MemberExpression {
	pub value1: String,
	pub value2: String,
}

pub struct ExprBinary {
	pub left: Box<Expression>,
	pub op: Operator,
	pub right: Box<Expression>,
}

pub struct ExprUnary {
	pub op: Operator,
	pub right: Box<Expression>,
}

pub struct ConstructorExpression {
	pub parameters: Vec<Expression>,
}
