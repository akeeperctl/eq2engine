//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Equilibrium Graphics File loader
//////////////////////////////////////////////////////////////////////////////////

#include <zlib.h>

#include "core/core_common.h"
#include "core/IFileSystem.h"
#include "modelloader_shared.h"

bool IsValidModelIdentifier(int id)
{
	if(EQUILIBRIUM_MODEL_SIGNATURE == id)
		return true;

	return false;
}

void ConvertHeaderToLatestVersion(basemodelheader_t* pHdr)
{

}

// loads all supported EGF model formats
studiohdr_t* Studio_LoadModel(const char* pszPath)
{
	
	IFile* file = g_fileSystem->Open(pszPath, "rb");

	if(!file)
	{
		MsgError("Can't open model file '%s'\n",pszPath);
		return nullptr;
	}

	const long len = file->GetSize();
	char* _buffer = (char*)PPAlloc(len+32); // +32 bytes for conversion issues

	file->Read(_buffer, 1, len);
	g_fileSystem->Close(file);

	basemodelheader_t* pBaseHdr = (basemodelheader_t*)_buffer;

	if(!IsValidModelIdentifier( pBaseHdr->ident ))
	{
		PPFree(_buffer);
		MsgError("Invalid model file '%s'\n",pszPath);
		return nullptr;
	}

	ConvertHeaderToLatestVersion( pBaseHdr );

	// TODO: Double data protection!!! (hash lookup)

	studiohdr_t* pHdr = (studiohdr_t*)pBaseHdr;

	if(pHdr->version != EQUILIBRIUM_MODEL_VERSION)
	{
		MsgError("Wrong model '%s' version, excepted %i, but model version is %i\n",pszPath, EQUILIBRIUM_MODEL_VERSION,pBaseHdr->version);
		PPFree(_buffer);
		return nullptr;
	}

	if(len != pHdr->length)
	{
		MsgError("Model is not valid (%d versus %d in header)!\n",len,pBaseHdr->size);
		PPFree(_buffer);
		return nullptr;
	}

	/*
#ifndef EDITOR
	char* str = (char*)stackalloc(strlen(pszPath+1));
	strcpy(str, pszPath);
	FixSlashes(str);

	if(stricmp(str, pHdr->modelName))
	{
		MsgError("Model %s is not valid model, didn't you replaced model?\n", pszPath);
		CacheFree(hunkHiMark);
		return nullptr;
	}
#endif
	*/


	return pHdr;
}

studioMotionData_t* Studio_LoadMotionData(const char* pszPath, int boneCount)
{
	ubyte* pData = (ubyte*)g_fileSystem->GetFileBuffer(pszPath);
	ubyte* pStart = pData;

	if(!pData)
		return nullptr;

	if (!boneCount)
		return nullptr;

	animpackagehdr_t* pHDR = (animpackagehdr_t*)pData;

	if(pHDR->ident != ANIMFILE_IDENT)
	{
		MsgError("%s: not a motion package file\n", pszPath);
		PPFree(pData);
		return nullptr;
	}

	if(pHDR->version != ANIMFILE_VERSION)
	{
		MsgError("Bad motion package version, please update or reinstall the game.\n", pszPath);
		PPFree(pData);
		return nullptr;
	}

	pData += sizeof(animpackagehdr_t);

	studioMotionData_t* pMotion = (studioMotionData_t*)PPAlloc(sizeof(studioMotionData_t));

	int numAnimDescs = 0;
	int numAnimFrames = 0;

	animationdesc_t*	animationdescs = nullptr;
	animframe_t*		animframes = nullptr;

	bool anim_frames_decompressed	= false;
	int nUncompressedFramesSize		= 0;

	// parse motion package
	for(int lump = 0; lump < pHDR->numLumps; lump++)
	{
		animpackagelump_t* pLump = (animpackagelump_t*)pData;
		pData += sizeof(animpackagelump_t);

		switch(pLump->type)
		{
			case ANIMFILE_ANIMATIONS:
			{
				numAnimDescs = pLump->size / sizeof(animationdesc_t);
				animationdescs = (animationdesc_t*)pData;
				break;
			}
			case ANIMFILE_ANIMATIONFRAMES:
			{
				numAnimFrames = pLump->size / sizeof(animframe_t);
				animframes = (animframe_t*)pData;
				anim_frames_decompressed = false;

				break;
			}
			case ANIMFILE_UNCOMPRESSEDFRAMESIZE:
			{
				nUncompressedFramesSize = *(int*)pData;
				break;
			}
			case ANIMFILE_COMPRESSEDFRAMES:
			{
				animframes = (animframe_t*)PPAlloc(nUncompressedFramesSize + 150);

				unsigned long realSize = nUncompressedFramesSize;

				int status = uncompress((ubyte*)animframes,&realSize,pData,pLump->size);

				if (status == Z_OK)
				{
					numAnimFrames = realSize / sizeof(animframe_t);
					anim_frames_decompressed = true;
				}
				else
					MsgError("ERROR! Cannot decompress animation frames from %s (error %d)!\n", pszPath, status);

				break;
			}
			case ANIMFILE_SEQUENCES:
			{
				pMotion->numsequences = pLump->size / sizeof(sequencedesc_t);

				pMotion->sequences = (sequencedesc_t*)PPAlloc(pLump->size);
				memcpy(pMotion->sequences, pData, pLump->size);
				break;
			}
			case ANIMFILE_EVENTS:
			{
				pMotion->numEvents = pLump->size / sizeof(sequenceevent_t);

				pMotion->events = (sequenceevent_t*)PPAlloc(pLump->size);
				memcpy(pMotion->events, pData, pLump->size);
				break;
			}
			case ANIMFILE_POSECONTROLLERS:
			{
				pMotion->numPoseControllers = pLump->size / sizeof(posecontroller_t);

				pMotion->poseControllers = (posecontroller_t*)PPAlloc(pLump->size);
				memcpy(pMotion->poseControllers, pData, pLump->size);
				break;
			}
		}

		pData += pLump->size;
	}

	// first processing done, convert animca animations to EGF format.
	pMotion->animations = PPAllocStructArray(studioAnimation_t, numAnimDescs);
	pMotion->numAnimations = numAnimDescs;

	pMotion->frames = PPAllocStructArray(animframe_t, numAnimFrames);
	memcpy(pMotion->frames, animframes, numAnimFrames * sizeof(animframe_t));

	for(int i = 0; i < pMotion->numAnimations; i++)
	{
		studioAnimation_t& anim = pMotion->animations[i];
		strcpy(anim.name, animationdescs[i].name);

		// determine frame count of animation
		const int numFrames = animationdescs[i].numFrames / boneCount;

		anim.bones = PPAllocStructArray(studioBoneFrame_t, boneCount);
		for(int j = 0; j < boneCount; j++)
		{
			anim.bones[j].numFrames = numFrames;
			anim.bones[j].keyFrames = pMotion->frames + (animationdescs[i].firstFrame + j*numFrames);
		}
	}

	if(anim_frames_decompressed)
	{
		PPFree(animframes);
	}

	PPFree(pStart);

	return pMotion;
}

bool Studio_LoadPhysModel(const char* pszPath, studioPhysData_t* pModel)
{
	memset(pModel, 0, sizeof(studioPhysData_t));

	if (!g_fileSystem->FileExist(pszPath))
		return false;

	ubyte* pData = (ubyte*)g_fileSystem->GetFileBuffer( pszPath );

	ubyte* pStart = pData;

	if(!pData)
		return false;

	physmodelhdr_t *pHdr = (physmodelhdr_t*)pData;

	if(pHdr->ident != PHYSFILE_ID)
	{
		MsgError("'%s' is not a POD physics model\n", pszPath);
		PPFree(pData);
		return false;
	}

	if(pHdr->version != PHYSFILE_VERSION)
	{
		MsgError("POD-File '%s' has physics model version\n", pszPath);
		PPFree(pData);
		return false;
	}

	Array<EqString> objectNames(PP_SL);

	pData += sizeof(physmodelhdr_t);

	int nLumps = pHdr->num_lumps;
	for(int lump = 0; lump < nLumps; lump++)
	{
		physmodellump_t* pLump = (physmodellump_t*)pData;
		pData += sizeof(physmodellump_t);

		switch(pLump->type)
		{
			case PHYSFILE_PROPERTIES:
			{
				physmodelprops_t* props = (physmodelprops_t*)pData;
				pModel->modeltype = props->model_usage;
				break;
			}
			case PHYSFILE_GEOMETRYINFO:
			{
				int numGeomInfos = pLump->size / sizeof(physgeominfo_t);

				physgeominfo_t* pGeomInfos = (physgeominfo_t*)pData;

				pModel->numShapes = numGeomInfos;
				pModel->shapes = PPAllocStructArray(studioPhysShapeCache_t, numGeomInfos);

				for(int i = 0; i < numGeomInfos; i++)
				{
					pModel->shapes[i].cachedata = nullptr;

					// copy shape info
					memcpy(&pModel->shapes[i].shape_info, &pGeomInfos[i], sizeof(physgeominfo_t));
				}
				break;
			}
			case PHYSFILE_OBJECTNAMES:
			{
				char* name = (char*)pData;

				int len = strlen(name);
				int sz = 0;

				do
				{
					char* str = name+sz;

					len = strlen(str);

					if(len > 0)
						objectNames.append(str);

					sz += len + 1;
				}while(sz < pLump->size);
				break;
			}
			case PHYSFILE_OBJECTS:
			{
				int numObjInfos = pLump->size / sizeof(physobject_t);
				physobject_t* physObjDataLump = (physobject_t*)pData;

				pModel->numObjects = numObjInfos;
				pModel->objects = PPAllocStructArray(studioPhysObject_t, numObjInfos);

				for(int i = 0; i < numObjInfos; i++)
				{
					studioPhysObject_t& objData = pModel->objects[i];

					if(objectNames.numElem() > 0)
						strcpy(objData.name, objectNames[i].ToCString());

					// copy shape info
					memcpy(&objData.object, &physObjDataLump[i], sizeof(physobject_t));

					for(int j = 0; j < MAX_PHYS_GEOM_PER_OBJECT; j++)
						objData.shapeCache[j] = nullptr;
				}
				break;
			}
			case PHYSFILE_JOINTDATA:
			{
				int numJointInfos = pLump->size / sizeof(physjoint_t);
				physjoint_t* pJointData = (physjoint_t*)pData;

				pModel->numJoints = numJointInfos;

				if(pModel->numJoints)
				{
					pModel->joints = (physjoint_t*)PPAlloc(pLump->size);
					memcpy(pModel->joints, pJointData, pLump->size );
				}
				break;
			}
			case PHYSFILE_VERTEXDATA:
			{
				int numVerts = pLump->size / sizeof(Vector3D);
				Vector3D* pVertexData = (Vector3D*)pData;

				pModel->numVertices = numVerts;
				pModel->vertices = (Vector3D*)PPAlloc(pLump->size);
				memcpy(pModel->vertices, pVertexData, pLump->size );
				break;
			}
			case PHYSFILE_INDEXDATA:
			{
				int numIndices = pLump->size / sizeof(int);
				int* pIndexData = (int*)pData;

				pModel->numIndices = numIndices;
				pModel->indices = (int*)PPAlloc(pLump->size);
				memcpy(pModel->indices, pIndexData, pLump->size );
				break;
			}
			default:
			{
				MsgWarning("*WARNING* Invalid POD-file '%s' lump type '%d'.\n", pszPath, pLump->type);
				break;
			}
		}

		pData += pLump->size;
	}

	PPFree(pStart);
	return true;
}

void Studio_FreeModel(studiohdr_t* pModel)
{
	PPFree(pModel);
}

void Studio_FreeAnimationData(studioAnimation_t* anim, int numBones)
{
	if (anim->bones) 
	{
		for (int i = 0; i < numBones; i++)
			PPFree(anim->bones[i].keyFrames);
	}

	PPFree(anim->bones);
}

void Studio_FreeMotionData(studioMotionData_t* data, int numBones)
{
	// NOTE: no need to delete bone keyFrames since they are mapped from data->frames.
	for (int i = 0; i < data->numAnimations; i++)
		PPFree(data->animations[i].bones);

	PPFree(data->frames);
	PPFree(data->sequences);
	PPFree(data->events);
	PPFree(data->poseControllers);
	PPFree(data->animations);
}

void Studio_FreePhysModel(studioPhysData_t* model)
{
	PPFree(model->indices);
	PPFree(model->vertices);
	PPFree(model->shapes);
	PPFree(model->objects);
	PPFree(model->joints);
}
