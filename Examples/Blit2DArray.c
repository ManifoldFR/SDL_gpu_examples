#include "Common.h"

static SDL_GpuGraphicsPipeline* Pipeline;
static SDL_GpuBuffer* VertexBuffer;
static SDL_GpuBuffer* IndexBuffer;
static SDL_GpuTexture* SourceTexture;
static SDL_GpuTexture* DestinationTexture;
static SDL_GpuSampler* Sampler;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	// Create the shaders
	SDL_GpuShader* vertexShader = LoadShader(context->Device, "TexturedQuad.vert", 0, 0, 0, 0);
	if (vertexShader == NULL)
	{
		SDL_Log("Failed to create vertex shader!");
		return -1;
	}

	SDL_GpuShader* fragmentShader = LoadShader(context->Device, "TexturedQuadArray.frag", 1, 0, 0, 0);
	if (fragmentShader == NULL)
	{
		SDL_Log("Failed to create fragment shader!");
		return -1;
	}

	// Create the pipeline
	SDL_GpuGraphicsPipelineCreateInfo pipelineCreateInfo = {
		.attachmentInfo = {
			.colorAttachmentCount = 1,
			.colorAttachmentDescriptions = (SDL_GpuColorAttachmentDescription[]){{
				.format = SDL_GetGpuSwapchainTextureFormat(context->Device, context->Window),
				.blendState = {
					.blendEnable = SDL_TRUE,
					.alphaBlendOp = SDL_GPU_BLENDOP_ADD,
					.colorBlendOp = SDL_GPU_BLENDOP_ADD,
					.colorWriteMask = 0xF,
					.srcColorBlendFactor = SDL_GPU_BLENDFACTOR_ONE,
					.srcAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ONE,
					.dstColorBlendFactor = SDL_GPU_BLENDFACTOR_ZERO,
					.dstAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ZERO
				}
			}},
		},
		.vertexInputState = (SDL_GpuVertexInputState){
			.vertexBindingCount = 1,
			.vertexBindings = (SDL_GpuVertexBinding[]){{
				.binding = 0,
				.inputRate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instanceStepRate = 0,
				.stride = sizeof(PositionTextureVertex)
			}},
			.vertexAttributeCount = 2,
			.vertexAttributes = (SDL_GpuVertexAttribute[]){{
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.location = 0,
				.offset = 0
			}, {
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
				.location = 1,
				.offset = sizeof(float) * 3
			}}
		},
		.multisampleState.sampleMask = 0xFFFF,
		.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertexShader = vertexShader,
		.fragmentShader = fragmentShader
	};

	Pipeline = SDL_CreateGpuGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (Pipeline == NULL)
	{
		SDL_Log("Failed to create pipeline!");
		return -1;
	}

	SDL_ReleaseGpuShader(context->Device, vertexShader);
	SDL_ReleaseGpuShader(context->Device, fragmentShader);

	// Load the images
	SDL_Surface *imageData1 = LoadImage("ravioli.bmp", 4);
	if (imageData1 == NULL)
	{
		SDL_Log("Could not load first image data!");
		return -1;
	}

	SDL_Surface *imageData2 = LoadImage("ravioli_inverted.bmp", 4);
	if (imageData2 == NULL)
	{
		SDL_Log("Could not load second image data!");
		return -1;
	}

	SDL_assert(imageData1->w == imageData2->w);
	SDL_assert(imageData1->h == imageData2->h);

	Uint32 srcWidth = imageData1->w;
	Uint32 srcHeight = imageData1->h;

	// Create the GPU resources
	VertexBuffer = SDL_CreateGpuBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
			.sizeInBytes = sizeof(PositionTextureVertex) * 8
		}
	);

	IndexBuffer = SDL_CreateGpuBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_INDEX_BIT,
			.sizeInBytes = sizeof(Uint16) * 6
		}
	);

	SourceTexture = SDL_CreateGpuTexture(context->Device, &(SDL_GpuTextureCreateInfo){
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
		.width = srcWidth,
		.height = srcHeight,
		.layerCountOrDepth = 2,
		.levelCount = 1,
		.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT
	});

	DestinationTexture = SDL_CreateGpuTexture(context->Device, &(SDL_GpuTextureCreateInfo){
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
		.width = srcWidth / 2,
		.height = srcHeight / 2,
		.layerCountOrDepth = 2,
		.levelCount = 1,
		.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT
	});

	Sampler = SDL_CreateGpuSampler(context->Device, &(SDL_GpuSamplerCreateInfo){
		.minFilter = SDL_GPU_FILTER_NEAREST,
		.magFilter = SDL_GPU_FILTER_NEAREST,
		.mipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		.addressModeU = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.addressModeV = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.addressModeW = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	});

	// Set up buffer data
	SDL_GpuTransferBuffer* bufferTransferBuffer = SDL_CreateGpuTransferBuffer(
		context->Device,
		&(SDL_GpuTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = (sizeof(PositionTextureVertex) * 8) + (sizeof(Uint16) * 6)
		}
	);

	PositionTextureVertex* transferData = SDL_MapGpuTransferBuffer(
		context->Device,
		bufferTransferBuffer,
		SDL_FALSE
	);

	transferData[0] = (PositionTextureVertex) { -1,  1, 0, 0, 0 };
	transferData[1] = (PositionTextureVertex) {  0,  1, 0, 1, 0 };
	transferData[2] = (PositionTextureVertex) {  0, -1, 0, 1, 1 };
	transferData[3] = (PositionTextureVertex) { -1, -1, 0, 0, 1 };
	transferData[4] = (PositionTextureVertex) {  0,  1, 0, 0, 0 };
	transferData[5] = (PositionTextureVertex) {  1,  1, 0, 1, 0 };
	transferData[6] = (PositionTextureVertex) {  1, -1, 0, 1, 1 };
	transferData[7] = (PositionTextureVertex) {  0, -1, 0, 0, 1 };

	Uint16* indexData = (Uint16*) &transferData[8];
	indexData[0] = 0;
	indexData[1] = 1;
	indexData[2] = 2;
	indexData[3] = 0;
	indexData[4] = 2;
	indexData[5] = 3;

	SDL_UnmapGpuTransferBuffer(context->Device, bufferTransferBuffer);

	// Set up texture data
	const Uint32 imageSizeInBytes = srcWidth * srcHeight * 4;
	SDL_GpuTransferBuffer* textureTransferBuffer = SDL_CreateGpuTransferBuffer(
		context->Device,
		&(SDL_GpuTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = imageSizeInBytes * 2
		}
	);

	Uint8* textureTransferPtr = SDL_MapGpuTransferBuffer(
		context->Device,
		textureTransferBuffer,
		SDL_FALSE
	);
	SDL_memcpy(textureTransferPtr, imageData1->pixels, imageSizeInBytes);
	SDL_memcpy(textureTransferPtr + imageSizeInBytes, imageData2->pixels, imageSizeInBytes);
	SDL_UnmapGpuTransferBuffer(context->Device, textureTransferBuffer);

	// Upload the transfer data to the GPU resources
	SDL_GpuCommandBuffer* uploadCmdBuf = SDL_AcquireGpuCommandBuffer(context->Device);
	SDL_GpuCopyPass* copyPass = SDL_BeginGpuCopyPass(uploadCmdBuf);

	SDL_UploadToGpuBuffer(
		copyPass,
		&(SDL_GpuTransferBufferLocation) {
			.transferBuffer = bufferTransferBuffer,
			.offset = 0
		},
		&(SDL_GpuBufferRegion) {
			.buffer = VertexBuffer,
			.offset = 0,
			.size = sizeof(PositionTextureVertex) * 8
		},
		SDL_FALSE
	);

	SDL_UploadToGpuBuffer(
		copyPass,
		&(SDL_GpuTransferBufferLocation) {
			.transferBuffer = bufferTransferBuffer,
			.offset = sizeof(PositionTextureVertex) * 8
		},
		&(SDL_GpuBufferRegion) {
			.buffer = IndexBuffer,
			.offset = 0,
			.size = sizeof(Uint16) * 6
		},
		SDL_FALSE
	);

	SDL_UploadToGpuTexture(
		copyPass,
		&(SDL_GpuTextureTransferInfo) {
			.transferBuffer = textureTransferBuffer,
			.offset = 0,
		},
		&(SDL_GpuTextureRegion){
			.texture = SourceTexture,
			.layer = 0,
			.w = srcWidth,
			.h = srcHeight,
			.d = 1
		},
		SDL_FALSE
	);

	SDL_UploadToGpuTexture(
		copyPass,
		&(SDL_GpuTextureTransferInfo) {
			.transferBuffer = textureTransferBuffer,
			.offset = imageSizeInBytes,
		},
		&(SDL_GpuTextureRegion){
			.texture = SourceTexture,
			.layer = 1,
			.w = srcWidth,
			.h = srcHeight,
			.d = 1
		},
		SDL_FALSE
	);

	SDL_DestroySurface(imageData1);
	SDL_DestroySurface(imageData2);
	SDL_EndGpuCopyPass(copyPass);

	SDL_BlitGpu(
		uploadCmdBuf,
		&(SDL_BlitGpuRegion){
			.texture = SourceTexture,
			.layerOrDepthPlane = 0,
			.w = srcWidth,
			.h = srcHeight,
		},
		&(SDL_BlitGpuRegion){
			.texture = DestinationTexture,
			.layerOrDepthPlane = 0,
			.w = srcWidth / 2,
			.h = srcHeight / 2,
		},
		SDL_FLIP_NONE,
		SDL_GPU_FILTER_LINEAR,
		SDL_FALSE
	);
	SDL_BlitGpu(
		uploadCmdBuf,
		&(SDL_BlitGpuRegion){
			.texture = SourceTexture,
			.layerOrDepthPlane = 1,
			.w = srcWidth,
			.h = srcHeight,
		},
		&(SDL_BlitGpuRegion){
			.texture = DestinationTexture,
			.layerOrDepthPlane = 1,
			.w = srcWidth / 2,
			.h = srcHeight / 2,
		},
		SDL_FLIP_NONE,
		SDL_GPU_FILTER_LINEAR,
		SDL_FALSE
	);

	SDL_SubmitGpu(uploadCmdBuf);
	SDL_ReleaseGpuTransferBuffer(context->Device, bufferTransferBuffer);
	SDL_ReleaseGpuTransferBuffer(context->Device, textureTransferBuffer);

	return 0;
}

static int Update(Context* context)
{
	return 0;
}

static int Draw(Context* context)
{
	SDL_GpuCommandBuffer* cmdbuf = SDL_AcquireGpuCommandBuffer(context->Device);
	if (cmdbuf == NULL)
	{
		SDL_Log("GpuAcquireCommandBuffer failed");
		return -1;
	}

	Uint32 w, h;
	SDL_GpuTexture* swapchainTexture = SDL_AcquireGpuSwapchainTexture(cmdbuf, context->Window, &w, &h);
	if (swapchainTexture != NULL)
	{
		SDL_GpuColorAttachmentInfo colorAttachmentInfo = { 0 };
		colorAttachmentInfo.texture = swapchainTexture;
		colorAttachmentInfo.clearColor = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_CLEAR;
		colorAttachmentInfo.storeOp = SDL_GPU_STOREOP_STORE;

		SDL_GpuRenderPass* renderPass = SDL_BeginGpuRenderPass(cmdbuf, &colorAttachmentInfo, 1, NULL);

		SDL_BindGpuGraphicsPipeline(renderPass, Pipeline);
		SDL_BindGpuVertexBuffers(renderPass, 0, &(SDL_GpuBufferBinding){ .buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_BindGpuIndexBuffer(renderPass, &(SDL_GpuBufferBinding){ .buffer = IndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_BindGpuFragmentSamplers(renderPass, 0, &(SDL_GpuTextureSamplerBinding){ .texture = SourceTexture, .sampler = Sampler }, 1);
		SDL_DrawGpuIndexedPrimitives(renderPass, 6, 1, 0, 0, 0);
		SDL_BindGpuFragmentSamplers(renderPass, 0, &(SDL_GpuTextureSamplerBinding){.texture = DestinationTexture, .sampler = Sampler }, 1);
		SDL_DrawGpuIndexedPrimitives(renderPass, 6, 1, 0, 4, 0);

		SDL_EndGpuRenderPass(renderPass);
	}

	SDL_SubmitGpu(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_ReleaseGpuGraphicsPipeline(context->Device, Pipeline);
	SDL_ReleaseGpuBuffer(context->Device, VertexBuffer);
	SDL_ReleaseGpuBuffer(context->Device, IndexBuffer);
	SDL_ReleaseGpuTexture(context->Device, SourceTexture);
	SDL_ReleaseGpuTexture(context->Device, DestinationTexture);
	SDL_ReleaseGpuSampler(context->Device, Sampler);

	CommonQuit(context);
}

Example Blit2DArray_Example = { "Blit2DArray", Init, Update, Draw, Quit };
