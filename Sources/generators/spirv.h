#pragma once

enum GLSLstd450 {
	GLSLstd450InverseSqrt,
	GLSLstd450Fract,
	GLSLstd450FMix,
	GLSLstd450Atan2,
	GLSLstd450MatrixInverse,
	GLSLstd450FAbs,
	GLSLstd450Normalize,
	GLSLstd450FClamp,
	GLSLstd450Pow,
	GLSLstd450FMin,
	GLSLstd450Cross,
	GLSLstd450Sin,
	GLSLstd450Cos,
	GLSLstd450Tan,
	GLSLstd450Asin,
	GLSLstd450Sqrt,
	GLSLstd450Length,
	GLSLstd450Exp2,
	GLSLstd450Distance,
	GLSLstd450Floor,
	GLSLstd450Exp,
	GLSLstd450Log2,
	GLSLstd450FSign,
	GLSLstd450Ceil,
	GLSLstd450FMax,
	GLSLstd450Step,
	GLSLstd450SmoothStep,
	GLSLstd450Reflect,
	GLSLstd450Refract,
	GLSLstd450Acos,
	GLSLstd450Atan,
	GLSLstd450Determinant
};

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
	OpLine,
	OpTypePointer,
	OpTypeFloat,
	OpTypeInt,
	OpTypeBool,
	OpTypeStruct,
	OpTypeSampler,
	OpTypeSampledImage,
	OpName,
	OpMemberName,
	OpDecorate,
	OpAccessChain,
	OpImageSampleExplicitLod,
	OpDPdx,
	OpDPdy,
	OpFwidth,
	OpTranspose,
	OpExtInst,
	OpTypeVoid,
	OpMemberDecorate,
	OpConstant,
	OpConstantTrue,
	OpConstantFalse,
	OpConstantComposite,
	OpFunctionEnd,
	OpPhi,
	OpCompositeExtract,
	OpVectorShuffle,
	OpFMul,
	OpIMul,
	OpFAdd,
	OpMatrixTimesScalar,
	OpVectorTimesScalar,
	OpFOrdGreaterThan,
	OpFOrdLessThanEqual,
	OpFOrdNotEqual,
	OpSGreaterThan,
	OpSGreaterThanEqual,
	OpLogicalAnd,
	OpFSub,
	OpDot,
	OpFDiv,
	OpSelect,
	OpCompositeInsert,
	OpFunctionCall,
	OpFunctionParameter,
	OpBranch,
	OpSelectionMerge,
	OpLoopMerge,
	OpBranchConditional,
	OpReturnValue,
	OpTypeFunction,
	OpIEqual,
	OpIAdd,
	OpFOrdLessThan,
	OpSLessThan,
	OpSLessThanEqual,
	OpFNegate,
	OpBitcast,
	OpConvertUToF,
	OpLoad,
	OpFOrdEqual,
	OpFOrdGreaterThanEqual,
	OpFMod,
	OpISub,
	OpLogicalOr,
	OpConvertFToS,
	OpLogicalNot,
	OpFunction,
	OpImageSampleDrefImplicitLod,
	OpUndef,
	OpKill,
	OpEntryPoint,
	OpMemoryModel,
	OpExtInstImport,
	OpSource,
	OpCapability,
	OpString,
	OpSourceExtension
};

enum Decoration { DecorationBuiltIn, DecorationColMajor, DecorationRowMajor, DecorationLocation, DecorationDescriptorSet, DecorationBinding };

enum ExecutionModel {
	ExecutionModelVertex,
	ExecutionModelTessellationControl,
	ExecutionModelTessellationEvaluation,
	ExecutionModelGeometry,
	ExecutionModelFragment,
	ExecutionModelGLCompute,
	ExecutionModelKernel
};

enum StorageClass { StorageClassInput, StorageClassOutput, StorageClassFunction, StorageClassUniformConstant, StorageClassUniform, StorageClassPushConstant };

enum ExecutionMode {
	ExecutionModeLocalSize,
	ExecutionModeInvocations,
	ExecutionModeTriangles,
	ExecutionModeOutputTriangleStrip,
	ExecutionModeSpacingEqual,
	ExecutionModeVertexOrderCw,
	ExecutionModeOutputVertices,
	ExecutionModeSpacingFractionalEven,
	ExecutionModeSpacingFractionalOdd,
	ExecutionModeVertexOrderCcw,
	ExecutionModePixelCenterInteger,
	ExecutionModeOriginUpperLeft,
	ExecutionModeOriginLowerLeft,
	ExecutionModeEarlyFragmentTests,
	ExecutionModePointMode,
	ExecutionModeXfb,
	ExecutionModeDepthReplacing,
	ExecutionModeDepthGreater,
	ExecutionModeDepthLess,
	ExecutionModeDepthUnchanged,
	ExecutionModeLocalSizeHint,
	ExecutionModeInputPoints,
	ExecutionModeInputLines,
	ExecutionModeInputLinesAdjacency,
	ExecutionModeInputTrianglesAdjacency,
	ExecutionModeQuads,
	ExecutionModeIsolines,
	ExecutionModeOutputPoints,
	ExecutionModeOutputLineStrip,
	ExecutionModeVecTypeHint,
	ExecutionModeContractionOff
};

enum BuiltIn {
	BuiltInFragDepth,
	BuiltInVertexId,
	BuiltInInstanceId,
	BuiltInVertexIndex,
	BuiltInInstanceIndex,
	BuiltInClipDistance,
	BuiltInPointSize,
	BuiltInPosition,
	BuiltInFrontFacing,
	BuiltInPointCoord,
	BuiltInSamplePosition,
	BuiltInSampleId,
	BuiltInSampleMask
};

enum Dim { Dim1D, Dim2D, Dim3D, DimCube };

enum ImageOperandsShift { ImageOperandsGradShift };

enum EShLanguage { EShLangVertex, EShLangFragment, EShLangGeometry, EShLangTessControl, EShLangTessEvaluation, EShLangCompute };

enum ImageOperands { ImageOperandsBiasShift, ImageOperandsLodShift, ImageOperandsOffsetShift, ImageOperandsConstOffsetsShift };