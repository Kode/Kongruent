#pragma once

enum GLSLstd450 { GLSLstd450InverseSqrt, GLSLstd450Fract, GLSLstd450FMix, GLSLstd450Atan2 };

enum Opcode {
	OpLabel,
	OpExecutionMode,
	OpTypeArray,
	OpTypeVector,
	OpTypeMatrix,
	OpTypeImage,
	OpImageWrite,
	OpVariable,
	OpCompositeConstruct,
	OpMatrixTimesVector,
	OpVectorTimesMatrix,
	OpMatrixTimesMatrix,
	OpImageSampleImplicitLod,
	OpConvertSToF,
	OpEmitVertex,
	OpEndPrimitive,
	OpReturn,
	OpStore,
	OpNop,
	OpLine
};

enum ExecutionModel {};

enum StorageClass { StorageClassInput, StorageClassOutput, StorageClassFunction, StorageClassUniformConstant };

enum ExecutionMode { ExecutionModeLocalSize };

enum BuiltIn {};

enum Dim { Dim2D };
