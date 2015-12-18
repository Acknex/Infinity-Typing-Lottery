#ifndef _DYNAMIC_MODELS_C_
#define _DYNAMIC_MODELS_C_

#include <acknex.h>
#include <d3d9.h>

DYNAMIC_MODEL* DynamicModel_Create()
{
	DYNAMIC_MODEL *model = sys_malloc(sizeof(DYNAMIC_MODEL));
	
	model->vertexCount = 0;
	model->faceCount = 0;
	model->skin[0] = NULL;
	model->skin[1] = NULL;
	model->skin[2] = NULL;
	model->skin[3] = NULL;
	
	return model;
}

void DynamicModel_Delete(DYNAMIC_MODEL *model)
{
	sys_free(model);
}

LPD3DXMESH DynamicModel_CreateMesh(DYNAMIC_MODEL *model)
{
	if(model == NULL) return 0;

	LPD3DXMESH mesh;
	
	if(model->vertexCount == 0)
	{
		return NULL;
	}
	if(model->faceCount == 0)
	{
		return NULL;
	}

	D3DVERTEX *vertexBuffer;
	short *indexBuffer;
	DWORD *attributeBufferResult;
	
	D3DXCreateMesh(model->faceCount,model->vertexCount,D3DXMESH_MANAGED,pvertexdecl,pd3ddev,&mesh);
	mesh->LockVertexBuffer(0, &vertexBuffer);
	mesh->LockIndexBuffer(0, &indexBuffer);
	mesh->LockAttributeBuffer(0, &attributeBufferResult);
	
	memcpy(vertexBuffer,model->vertexBuffer,model->vertexCount * sizeof(D3DVERTEX));
	memcpy(indexBuffer,model->indexBuffer,3 * model->faceCount * sizeof(short));
	memcpy(attributeBufferResult,model->attributeBuffer,model->vertexCount * sizeof(DWORD));
	
	mesh->UnlockVertexBuffer();
	mesh->UnlockIndexBuffer();
	mesh->UnlockAttributeBuffer();
	
	if((DynamicModels_Settings.flags & DYNAMIC_MODELS_OPTIMIZE) != 0)
	{
		D3DXWELDEPSILONS Epsilons;
		// Set epsilon values
		Epsilons.Normal = 0.001;
		Epsilons.Position = 0.1; 
		WeldVertices(	mesh,
						D3DXWELDEPSILONS_WELDPARTIALMATCHES,
						&Epsilons,
						NULL,
						NULL,
						NULL,
						NULL);
	}
	
	if((DynamicModels_Settings.flags & DYNAMIC_MODELS_FIXNORMALS) != 0)
	{
		D3DXComputeNormals((LPD3DXBASEMESH)mesh, NULL);
	}
	
	return mesh;
}

ENTITY* DynamicModel_CreateInstance(DYNAMIC_MODEL *model, VECTOR *pos, EVENT act)
{
	if(model == NULL) return NULL;
	
	ENTITY* ent = ent_create(CUBE_MDL,pos,NULL);
	ent_clone(ent);
	int stage = 0;
	
	LPD3DXMESH mesh = DynamicModel_CreateMesh(model);
	
	if(mesh == NULL) return NULL;
	
	ent_setmesh(ent,mesh,0,stage);
	
	int i;
	for(i = 0; i < 4; i++)
	{
		BMAP* skinSource = model->skin[i];
		if(skinSource)
		{
			if(DynamicModels_Settings.flags & DYNAMIC_MODELS_CLONE_TEX)
			{
				BMAP* skinTarget = bmap_createblack(bmap_width(skinSource),bmap_height(skinSource),24);			
				bmap_blit(skinTarget,skinSource,NULL,NULL);
				ent_setskin(ent,skinTarget,i + 1);
			}
			else
			{
				ent_setskin(ent,skinSource,i + 1);
			}
		}
	}
	
	if(act != NULL)
	{
		me = ent;
		DynamicModel_TemporaryAction = act;
		DynamicModel_TemporaryAction();
		me = NULL;
	}
	return ent;
}

void DynamicModel_AddFace(DYNAMIC_MODEL *model, DYNAMIC_FACE *face)
{
	if(model == NULL) return;
	if(face == NULL) return;
	
	int iVertexStartOffset = model->vertexCount;
	int iFaceStartOffset = model->faceCount;
	int i;
	// Copy vertices
	for(i = 0; i < 3; i++)
	{
		D3DVERTEX* vSource = &(face->v[i]);
		D3DVERTEX* vTarget = &(model->vertexBuffer[iVertexStartOffset + i]);
		memcpy(vTarget,vSource,sizeof(D3DVERTEX));
		model->indexBuffer[3 * iFaceStartOffset + i] = iVertexStartOffset + i;
	}
	
	model->vertexCount += 3;
	model->faceCount += 1;
}

void DynamicModel_AddQuad(DYNAMIC_MODEL *model, DYNAMIC_QUAD *quad)
{
	if(model == NULL) return;
	if(quad == NULL) return;
	
	short vertices[4];
	int iVertexStartOffset = model->vertexCount;
	int iFaceStartOffset = model->faceCount;
	int i;
	
	// Copy vertices
	for(i = 0; i < 4; i++)
	{
		D3DVERTEX* vSource = &(quad->v[i]);
		D3DVERTEX* vTarget = &(model->vertexBuffer[iVertexStartOffset + i]);
		memcpy(vTarget,vSource,sizeof(D3DVERTEX));
		
		vertices[i] = iVertexStartOffset + i;
	}
	
	// Face 1
	model->indexBuffer[3 * iFaceStartOffset + 0] = vertices[2];
	model->indexBuffer[3 * iFaceStartOffset + 1] = vertices[1];
	model->indexBuffer[3 * iFaceStartOffset + 2] = vertices[0];
	
	// Face 2
	model->indexBuffer[3 * iFaceStartOffset + 3] = vertices[1];
	model->indexBuffer[3 * iFaceStartOffset + 4] = vertices[2];
	model->indexBuffer[3 * iFaceStartOffset + 5] = vertices[3];
	
	// Index count per dynamic face
	model->vertexCount += 4;
	model->faceCount += 2;
}

void DynamicModel_AddEntity(DYNAMIC_MODEL *model, ENTITY *ent)
{
	DynamicModel_AddMesh(model, ent_getmesh(ent, 0 ,0), &(ent->x), &(ent->pan), &(ent->scale_x));
}

void DynamicModel_AddMesh(DYNAMIC_MODEL *model, LPD3DXMESH mesh)
{
	DynamicModel_AddMesh(model, mesh, nullvector, nullvector, vector(1,1,1));
}

void DynamicModel_AddMesh(DYNAMIC_MODEL *model, LPD3DXMESH mesh, VECTOR *offset)
{
	DynamicModel_AddMesh(model, mesh, offset, nullvector, vector(1,1,1));
}

void DynamicModel_AddMesh(DYNAMIC_MODEL *model, LPD3DXMESH mesh, VECTOR *offset, ANGLE *rotation)
{
	DynamicModel_AddMesh(model, mesh, offset, rotation, vector(1,1,1));
}

void DynamicModel_AddMesh(DYNAMIC_MODEL *model, LPD3DXMESH mesh, VECTOR *offset, ANGLE *rotation, VECTOR *scale)
{
	if(model == NULL) return;
	if(mesh == NULL) return;
	
	D3DVERTEX *vertexBufferMesh;
	short *indexBufferMesh;
	DWORD *attributeBuffer;
	
	int meshVertexCount = mesh->GetNumVertices();
	int meshFaceCount = mesh->GetNumFaces();
	
	if(model->vertexCount + meshVertexCount > DYNAMIC_MODEL_MAX_VERTEXCOUNT)
		return;
	
	if(model->faceCount + meshFaceCount > (DYNAMIC_MODEL_MAX_INDEXCOUNT / 3))
		return;
		
	mesh->LockVertexBuffer(0, &vertexBufferMesh);
	mesh->LockIndexBuffer(0, &indexBufferMesh);
	mesh->LockAttributeBuffer(0, &attributeBuffer);
	
	int i;
	for(i = 0; i < meshVertexCount; i++)
	{
		memcpy(&(model->vertexBuffer[model->vertexCount + i]),vertexBufferMesh,sizeof(D3DVERTEX));
		
		VECTOR pos;
		pos.x = model->vertexBuffer[model->vertexCount + i].x;
		pos.z = model->vertexBuffer[model->vertexCount + i].y;
		pos.y = model->vertexBuffer[model->vertexCount + i].z;
		
		vec_mul(&pos, scale);
		vec_rotate(&pos, rotation);
		vec_add(&pos, offset);
		
		model->vertexBuffer[model->vertexCount + i].x = pos.x;
		model->vertexBuffer[model->vertexCount + i].z = pos.y;
		model->vertexBuffer[model->vertexCount + i].y = pos.z;
		
		normal.x = model->vertexBuffer[model->vertexCount + i].nx;
		normal.z = model->vertexBuffer[model->vertexCount + i].ny;
		normal.y = model->vertexBuffer[model->vertexCount + i].nz;
		
		vec_rotate(&normal, rotation);
		
		model->vertexBuffer[model->vertexCount + i].nx = normal.x;
		model->vertexBuffer[model->vertexCount + i].nz = normal.y;
		model->vertexBuffer[model->vertexCount + i].ny = normal.z;
		
		model->attributeBuffer[model->vertexCount + i] = *attributeBuffer;
		
		vertexBufferMesh++;
		attributeBuffer++;
	}
	
	for(i = 0; i < meshFaceCount * 3; i++)
	{
		model->indexBuffer[model->faceCount * 3 + i] = model->vertexCount + *indexBufferMesh;
		indexBufferMesh++;
	}
	
	mesh->UnlockVertexBuffer();
	mesh->UnlockIndexBuffer();
	mesh->UnlockAttributeBuffer();
	
	model->vertexCount += meshVertexCount;
	model->faceCount += meshFaceCount;
}

void DynamicModel_Save(DYNAMIC_MODEL *model, char *filename)
{
	LPD3DXMESH mesh = DynamicModel_CreateMesh(model);
	SaveMeshToX(
		filename,
		mesh,
		NULL,
		NULL,
		NULL,
		0,
		DynamicModels_Settings.xFormat);
}




















#endif