use super::expression::*;

pub enum Statement {
	Expression(Expression),
	If(IfStatement),
	Block(BlockStatement),
	Declaration(DeclarationStatement),
	PreprocessorDirective(PreprocessorStatement),
	Function(FunctionStatement),
	Struct(StructStatement),
}

pub struct DeclarationStatement {
	pub name: String,
	pub init: Option<Expression>,
}

pub struct BlockStatement {
	pub statements: Vec<Statement>,
}

pub struct FunctionStatement {
	pub parameters: Vec<String>,
	pub block: BlockStatement,
}

pub struct IfStatement {
	pub test: Expression,
	pub block: Box<Statement>,
}

pub struct CallStatement {
	pub func: Box<Expression>,
	pub parameters: Vec<Expression>,
}

pub struct PreprocessorStatement {
	pub name: String,
	pub parameters: Vec<Expression>,
}

pub struct Member {
	pub name: String,
	pub member_type: String,
}

pub struct StructStatement {
	pub attribute: String,
	pub name: String,
	pub members: Vec<Member>,
}
