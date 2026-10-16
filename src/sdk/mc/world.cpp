


#include "../../util/aes_string.hpp"
#include "../../util/logger.hpp"
#include "../core/minecraft.h"
#include "../jni/jni_cache.h"





int Minecraft::getBlockId(int x, int y, int z) {
    if (!isValid())
        return 0;

    if (y < 0 || y > 255)
        return 0;

    JNIEnv* env = Java::Env();
    if (!env)
        return 0;

    if (env->PushLocalFrame(16) < 0)
        return 0;

    int blockId = 0;

    
    if (Java::Version == MinecraftVersion::MC_1_7_10) {
        jobject world = getWorld();
        if (world) {
            
            if (JniCache::mid_getBlockWorld) {
                jobject block = env->CallObjectMethod(world, JniCache::mid_getBlockWorld, x, y, z);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    block = nullptr;
                }
                if (block) {
                    
                    if (JniCache::cls_Block && JniCache::mid_getIdFromBlock) {
                        blockId = env->CallStaticIntMethod(JniCache::cls_Block, JniCache::mid_getIdFromBlock, block);
                        if (env->ExceptionCheck()) {
                            env->ExceptionClear();
                            blockId = 0;
                        }
                    }
                }
            }
        }
    } else {
        jobject world = getWorld();
        if (world) {
            
            if (JniCache::cls_BlockPos && JniCache::mid_blockPosInit) {
                jobject blockPos = env->NewObject(JniCache::cls_BlockPos, JniCache::mid_blockPosInit, x, y, z);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    blockPos = nullptr;
                }
                if (blockPos && JniCache::mid_getBlockState) {
                    
                    jobject blockState = env->CallObjectMethod(world, JniCache::mid_getBlockState, blockPos);
                    if (env->ExceptionCheck()) {
                        env->ExceptionClear();
                        blockState = nullptr;
                    }
                    if (blockState && JniCache::mid_getBlock) {
                        
                        jobject block = env->CallObjectMethod(blockState, JniCache::mid_getBlock);
                        if (env->ExceptionCheck()) {
                            env->ExceptionClear();
                            block = nullptr;
                        }
                        if (block && JniCache::cls_Block && JniCache::mid_getIdFromBlock) {
                            
                            blockId = env->CallStaticIntMethod(JniCache::cls_Block, JniCache::mid_getIdFromBlock, block);
                            if (env->ExceptionCheck()) {
                                env->ExceptionClear();
                                blockId = 0;
                            }
                        }
                    }
                }
            }
        }
    }

    env->PopLocalFrame(nullptr);
    return blockId;
}

bool Minecraft::damageBlock(int x, int y, int z) {
    JNIEnv* env = Java::Env();
    if (!isValid() || !env || !JniCache::fid_playerController)
        return false;

    if (env->PushLocalFrame(16) < 0)
        return false;

    jobject controller = env->GetObjectField(instance, JniCache::fid_playerController);
    if (!controller) {
        env->PopLocalFrame(nullptr);
        return false;
    }

    jclass controllerClass = env->GetObjectClass(controller);
    if (!controllerClass) {
        env->PopLocalFrame(nullptr);
        return false;
    }

    bool damaged = false;
    if (Java::Version == MinecraftVersion::MC_1_7_10) {
        jmethodID damageMethod =
            env->GetMethodID(controllerClass, OBFUSCATE_STRING("onPlayerDamageBlock").c_str(),
                             OBFUSCATE_STRING("(IIII)Z").c_str());
        if (!damageMethod) {
            env->ExceptionClear();
            damageMethod = env->GetMethodID(controllerClass, OBFUSCATE_STRING("func_78759_c").c_str(),
                                            OBFUSCATE_STRING("(IIII)Z").c_str());
        }
        if (!damageMethod && env->ExceptionCheck())
            env->ExceptionClear();
        if (damageMethod)
            damaged = env->CallBooleanMethod(controller, damageMethod, x, y, z, 1) == JNI_TRUE;
    } else if (JniCache::cls_BlockPos && JniCache::mid_blockPosInit) {
        jmethodID damageMethod = env->GetMethodID(
            controllerClass, OBFUSCATE_STRING("onPlayerDamageBlock").c_str(),
            OBFUSCATE_STRING("(Lnet/minecraft/util/BlockPos;Lnet/minecraft/util/EnumFacing;)Z").c_str());
        if (!damageMethod) {
            env->ExceptionClear();
            damageMethod = env->GetMethodID(
                controllerClass, OBFUSCATE_STRING("func_180512_c").c_str(),
                OBFUSCATE_STRING("(Lnet/minecraft/util/BlockPos;Lnet/minecraft/util/EnumFacing;)Z").c_str());
        }
        if (!damageMethod && env->ExceptionCheck())
            env->ExceptionClear();

        jclass facingClass = nullptr;
        if (damageMethod && Java::AssignClass(OBFUSCATE_STRING("net.minecraft.util.EnumFacing"), facingClass)) {
            jmethodID valuesMethod = env->GetStaticMethodID(
                facingClass, OBFUSCATE_STRING("values").c_str(),
                OBFUSCATE_STRING("()[Lnet/minecraft/util/EnumFacing;").c_str());
            if (!valuesMethod && env->ExceptionCheck())
                env->ExceptionClear();
            jobjectArray values = valuesMethod
                                      ? static_cast<jobjectArray>(env->CallStaticObjectMethod(facingClass, valuesMethod))
                                      : nullptr;
            jobject facingUp = values && env->GetArrayLength(values) > 1 ? env->GetObjectArrayElement(values, 1)
                                                                         : nullptr;
            jobject blockPos = env->NewObject(JniCache::cls_BlockPos, JniCache::mid_blockPosInit, x, y, z);
            if (blockPos && facingUp)
                damaged = env->CallBooleanMethod(controller, damageMethod, blockPos, facingUp) == JNI_TRUE;
        }
    }

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        damaged = false;
    }

    env->PopLocalFrame(nullptr);
    return damaged;
}





bool Minecraft::damageTargetedBlock() {
    JNIEnv* env = Java::Env();
    if (!isValid() || !env)
        return false;
    if (!JniCache::fid_playerController || !JniCache::fid_objectMouseOver || !JniCache::mid_getBlockPos ||
        !JniCache::cls_MovingObjectPosition)
        return false;

    if (env->PushLocalFrame(16) < 0)
        return false;

    bool damaged = false;
    do {
        jobject controller = env->GetObjectField(instance, JniCache::fid_playerController);
        if (!controller)
            break;

        jobject mouseOver = env->GetObjectField(instance, JniCache::fid_objectMouseOver);
        if (!mouseOver)
            break;

        
        jobject blockPos = env->CallObjectMethod(mouseOver, JniCache::mid_getBlockPos);
        if (!blockPos || env->ExceptionCheck()) {
            env->ExceptionClear();
            break;
        }

        
        
        
        jfieldID fidSideHit =
            env->GetFieldID(JniCache::cls_MovingObjectPosition, OBFUSCATE_STRING("sideHit").c_str(),
                            OBFUSCATE_STRING("Lnet/minecraft/util/EnumFacing;").c_str());
        if (!fidSideHit) {
            env->ExceptionClear();
            fidSideHit =
                env->GetFieldID(JniCache::cls_MovingObjectPosition, OBFUSCATE_STRING("field_178784_b").c_str(),
                                OBFUSCATE_STRING("Lnet/minecraft/util/EnumFacing;").c_str());
        }
        if (!fidSideHit) {
            env->ExceptionClear();
            break;
        }
        jobject sideHit = env->GetObjectField(mouseOver, fidSideHit);
        if (!sideHit)
            break;

        jclass controllerClass = env->GetObjectClass(controller);
        if (!controllerClass)
            break;

        jmethodID damageMethod = env->GetMethodID(
            controllerClass, OBFUSCATE_STRING("onPlayerDamageBlock").c_str(),
            OBFUSCATE_STRING("(Lnet/minecraft/util/BlockPos;Lnet/minecraft/util/EnumFacing;)Z").c_str());
        if (!damageMethod) {
            env->ExceptionClear();
            damageMethod = env->GetMethodID(
                controllerClass, OBFUSCATE_STRING("func_180512_c").c_str(),
                OBFUSCATE_STRING("(Lnet/minecraft/util/BlockPos;Lnet/minecraft/util/EnumFacing;)Z").c_str());
        }
        if (!damageMethod) {
            env->ExceptionClear();
            break;
        }

        damaged = env->CallBooleanMethod(controller, damageMethod, blockPos, sideHit) == JNI_TRUE;
    } while (false);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        damaged = false;
    }

    env->PopLocalFrame(nullptr);
    return damaged;
}





Minecraft::RAYTRACE_BLOCKS_RESULT Minecraft::rayTraceBlocks(Vec3 from, Vec3 direction, Vec3& result,
                                                            bool stopOnLiquid) {
    result = {-1.5f, -1.5f, -1.5f};  

    if (!isValid())
        return RAYTRACE_BLOCKS_RESULT::RAYTRACE_ERROR;

    JNIEnv* env = Java::Env();
    if (!env)
        return RAYTRACE_BLOCKS_RESULT::RAYTRACE_ERROR;

    if (env->PushLocalFrame(16) < 0)
        return RAYTRACE_BLOCKS_RESULT::RAYTRACE_ERROR;

    jobject world = getWorld();
    if (!world) {
        env->PopLocalFrame(nullptr);
        return RAYTRACE_BLOCKS_RESULT::RAYTRACE_ERROR;
    }

    
    if (!JniCache::cls_Vec3 || !JniCache::mid_vec3Init) {
        env->PopLocalFrame(nullptr);
        return RAYTRACE_BLOCKS_RESULT::RAYTRACE_ERROR;
    }

    jobject fromObj =
        env->NewObject(JniCache::cls_Vec3, JniCache::mid_vec3Init, (double)from.x, (double)from.y, (double)from.z);
    if (!fromObj) {
        env->PopLocalFrame(nullptr);
        return RAYTRACE_BLOCKS_RESULT::RAYTRACE_ERROR;
    }

    jobject directionObj = env->NewObject(JniCache::cls_Vec3, JniCache::mid_vec3Init, (double)direction.x,
                                          (double)direction.y, (double)direction.z);
    if (!directionObj) {
        env->PopLocalFrame(nullptr);
        return RAYTRACE_BLOCKS_RESULT::RAYTRACE_ERROR;
    }

    
    jobject mop = nullptr;
    if (JniCache::mid_rayTraceBlocks) {
        mop = env->CallObjectMethod(world, JniCache::mid_rayTraceBlocks, fromObj, directionObj, stopOnLiquid);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            
            mop = env->CallObjectMethod(world, JniCache::mid_rayTraceBlocks, fromObj, directionObj);
        }
    }

    
    if (!mop) {
        env->PopLocalFrame(nullptr);
        return RAYTRACE_BLOCKS_RESULT::NO_HIT;
    }

    
    bool gotPosition = false;

    
    if (JniCache::mid_getBlockPos && JniCache::mid_getX && JniCache::mid_getY && JniCache::mid_getZ) {
        jobject blockPosObj = env->CallObjectMethod(mop, JniCache::mid_getBlockPos);
        if (blockPosObj) {
            result.x = (float)env->CallIntMethod(blockPosObj, JniCache::mid_getX);
            result.y = (float)env->CallIntMethod(blockPosObj, JniCache::mid_getY);
            result.z = (float)env->CallIntMethod(blockPosObj, JniCache::mid_getZ);
            gotPosition = true;
        }
    }

    
    if (!gotPosition && JniCache::fid_blockX && JniCache::fid_blockY && JniCache::fid_blockZ) {
        result.x = (float)env->GetIntField(mop, JniCache::fid_blockX);
        result.y = (float)env->GetIntField(mop, JniCache::fid_blockY);
        result.z = (float)env->GetIntField(mop, JniCache::fid_blockZ);
        gotPosition = true;
    }

    env->PopLocalFrame(nullptr);

    if (gotPosition) {
        return RAYTRACE_BLOCKS_RESULT::HIT;
    }

    return RAYTRACE_BLOCKS_RESULT::RAYTRACE_ERROR;
}












void Minecraft::captureBlocks(int radiusXZ, int radiusY, int originX, int originY, int originZ, size_t maxBlocks,
                              std::vector<BlockSample>& out) {
    out.clear();
    if (!isValid())
        return;
    
    if (Java::Version == MinecraftVersion::MC_1_7_10)
        return;

    JNIEnv* env = Java::Env();
    if (!env)
        return;

    jobject world = getWorld();
    if (!world)
        return;

    
    static int state = 0;
    static jmethodID mid_getChunk = nullptr;
    static jmethodID mid_getStorage = nullptr;
    static jmethodID mid_getData = nullptr;
    if (state == 2)
        return;

    if (!mid_getChunk) {
        jclass worldCls = env->GetObjectClass(world);
        mid_getChunk = env->GetMethodID(worldCls, OBFUSCATE_STRING("getChunkFromChunkCoords").c_str(),
                                        OBFUSCATE_STRING("(II)Lnet/minecraft/world/chunk/Chunk;").c_str());
        if (!mid_getChunk) {
            env->ExceptionClear();
            mid_getChunk = env->GetMethodID(worldCls, OBFUSCATE_STRING("func_72964_e").c_str(),
                                            OBFUSCATE_STRING("(II)Lnet/minecraft/world/chunk/Chunk;").c_str());
        }
        if (worldCls)
            env->DeleteLocalRef(worldCls);
        if (!mid_getChunk) {
            env->ExceptionClear();
            state = 2;
            return;
        }
        state = 1;
    }

    const int minX = originX - radiusXZ, maxX = originX + radiusXZ;
    const int minZ = originZ - radiusXZ, maxZ = originZ + radiusXZ;
    const int minY = (originY - radiusY < 0) ? 0 : (originY - radiusY);
    const int maxY = (originY + radiusY > 255) ? 255 : (originY + radiusY);
    const int cxMin = minX >> 4, cxMax = maxX >> 4;
    const int czMin = minZ >> 4, czMax = maxZ >> 4;

    for (int cx = cxMin; cx <= cxMax; ++cx) {
        for (int cz = czMin; cz <= czMax; ++cz) {
            if (out.size() >= maxBlocks)
                return;
            if (env->PushLocalFrame(64) < 0)
                return;

            jobject chunk = env->CallObjectMethod(world, mid_getChunk, cx, cz);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                chunk = nullptr;
            }
            if (!chunk) {
                env->PopLocalFrame(nullptr);
                continue;
            }

            if (!mid_getStorage) {
                jclass chunkCls = env->GetObjectClass(chunk);
                mid_getStorage =
                    env->GetMethodID(chunkCls, OBFUSCATE_STRING("getBlockStorageArray").c_str(),
                                     OBFUSCATE_STRING("()[Lnet/minecraft/world/chunk/storage/ExtendedBlockStorage;").c_str());
                if (!mid_getStorage) {
                    env->ExceptionClear();
                    mid_getStorage =
                        env->GetMethodID(chunkCls, OBFUSCATE_STRING("func_76587_i").c_str(),
                                         OBFUSCATE_STRING("()[Lnet/minecraft/world/chunk/storage/ExtendedBlockStorage;").c_str());
                }
                if (chunkCls)
                    env->DeleteLocalRef(chunkCls);
                if (!mid_getStorage) {
                    env->ExceptionClear();
                    env->PopLocalFrame(nullptr);
                    state = 2;
                    return;
                }
            }

            jobjectArray storArray = (jobjectArray)env->CallObjectMethod(chunk, mid_getStorage);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                storArray = nullptr;
            }
            if (!storArray) {
                env->PopLocalFrame(nullptr);
                continue;
            }

            const jsize nSec = env->GetArrayLength(storArray);
            for (jsize si = 0; si < nSec; ++si) {
                const int baseY = (int)si * 16;
                if (baseY + 15 < minY || baseY > maxY)
                    continue;

                jobject section = env->GetObjectArrayElement(storArray, si);
                if (!section)
                    continue;  

                if (!mid_getData) {
                    jclass secCls = env->GetObjectClass(section);
                    mid_getData = env->GetMethodID(secCls, OBFUSCATE_STRING("getData").c_str(),
                                                   OBFUSCATE_STRING("()[C").c_str());
                    if (!mid_getData) {
                        env->ExceptionClear();
                        mid_getData = env->GetMethodID(secCls, OBFUSCATE_STRING("func_177487_g").c_str(),
                                                       OBFUSCATE_STRING("()[C").c_str());
                    }
                    if (secCls)
                        env->DeleteLocalRef(secCls);
                    if (!mid_getData) {
                        env->ExceptionClear();
                        env->DeleteLocalRef(section);
                        env->PopLocalFrame(nullptr);
                        state = 2;
                        return;
                    }
                }

                jcharArray dataArr = (jcharArray)env->CallObjectMethod(section, mid_getData);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    dataArr = nullptr;
                }
                if (!dataArr) {
                    env->DeleteLocalRef(section);
                    continue;
                }

                if (env->GetArrayLength(dataArr) >= 4096) {
                    jchar buf[4096];
                    env->GetCharArrayRegion(dataArr, 0, 4096, buf);
                    if (env->ExceptionCheck()) {
                        env->ExceptionClear();
                    } else {
                        for (int ly = 0; ly < 16; ++ly) {
                            const int wy = baseY + ly;
                            if (wy < minY || wy > maxY)
                                continue;
                            for (int lz = 0; lz < 16; ++lz) {
                                const int wz = (cz << 4) + lz;
                                if (wz < minZ || wz > maxZ)
                                    continue;
                                for (int lx = 0; lx < 16; ++lx) {
                                    const int wx = (cx << 4) + lx;
                                    if (wx < minX || wx > maxX)
                                        continue;
                                    const jchar v = buf[(ly << 8) | (lz << 4) | lx];
                                    const int id = v >> 4;
                                    if (id == 0)
                                        continue;  
                                    BlockSample b;
                                    b.dx = (int16_t)(wx - originX);
                                    b.dy = (int16_t)(wy - originY);
                                    b.dz = (int16_t)(wz - originZ);
                                    b.id = (uint16_t)id;
                                    out.push_back(b);
                                    if (out.size() >= maxBlocks) {
                                        env->DeleteLocalRef(dataArr);
                                        env->DeleteLocalRef(section);
                                        env->PopLocalFrame(nullptr);
                                        return;
                                    }
                                }
                            }
                        }
                    }
                }
                env->DeleteLocalRef(dataArr);
                env->DeleteLocalRef(section);
            }
            env->PopLocalFrame(nullptr);
        }
    }
}
