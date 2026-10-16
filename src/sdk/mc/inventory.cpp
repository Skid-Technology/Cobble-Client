


#include "../../util/aes_string.hpp"
#include "../../util/logger.hpp"
#include "../core/minecraft.h"
#include "../jni/jni_cache.h"





int getItemIdFromStack(JNIEnv* env, jobject itemStack) {
    if (!itemStack || !JniCache::mid_getItem || !JniCache::cls_Item || !JniCache::mid_getIdFromItem)
        return -1;

    jobject item = env->CallObjectMethod(itemStack, JniCache::mid_getItem);
    if (!item)
        return -1;

    int id = env->CallStaticIntMethod(JniCache::cls_Item, JniCache::mid_getIdFromItem, item);
    env->DeleteLocalRef(item);
    return id;
}





std::vector<SlotInfo> Minecraft::getInventorySlots() {
    std::vector<SlotInfo> result;
    if (!isValid())
        return result;

    JNIEnv* env = Java::Env();
    if (!env)
        return result;

    jobject localPlayer = getPlayer();
    if (!localPlayer)
        return result;

    
    if (!JniCache::fid_inventory) {
        env->DeleteLocalRef(localPlayer);
        return result;
    }

    jobject inventory = env->GetObjectField(localPlayer, JniCache::fid_inventory);
    if (!inventory) {
        env->DeleteLocalRef(localPlayer);
        return result;
    }

    
    jclass inventoryCls = env->GetObjectClass(inventory);
    if (!inventoryCls) {
        env->DeleteLocalRef(inventory);
        env->DeleteLocalRef(localPlayer);
        return result;
    }

    
    jfieldID fid_mainInventory = env->GetFieldID(inventoryCls, OBFUSCATE_STRING("mainInventory").c_str(),
                                                 OBFUSCATE_STRING("[Lnet/minecraft/item/ItemStack;").c_str());
    if (!fid_mainInventory) {
        env->ExceptionClear();
        env->DeleteLocalRef(inventoryCls);
        env->DeleteLocalRef(inventory);
        env->DeleteLocalRef(localPlayer);
        return result;
    }

    jobjectArray mainInventory = (jobjectArray)env->GetObjectField(inventory, fid_mainInventory);
    if (!mainInventory) {
        env->DeleteLocalRef(inventoryCls);
        env->DeleteLocalRef(inventory);
        env->DeleteLocalRef(localPlayer);
        return result;
    }

    
    jclass nbtCompoundCls = nullptr;
    Java::AssignClass(OBFUSCATE_STRING("net.minecraft.nbt.NBTTagCompound"), nbtCompoundCls);
    jmethodID mid_getShort = nullptr;
    if (nbtCompoundCls) {
        mid_getShort = env->GetMethodID(nbtCompoundCls, OBFUSCATE_STRING("getShort").c_str(),
                                        OBFUSCATE_STRING("(Ljava/lang/String;)S").c_str());
    }

    
    jstring strId = env->NewStringUTF(OBFUSCATE_STRING("id").c_str());
    jstring strLvl = env->NewStringUTF(OBFUSCATE_STRING("lvl").c_str());

    int arrayLen = env->GetArrayLength(mainInventory);

    
    for (int i = 0; i < arrayLen && i < 36; i++) {
        SlotInfo slot;
        slot.slotId = i;

        jobject itemStack = env->GetObjectArrayElement(mainInventory, i);
        if (!itemStack) {
            result.push_back(slot);
            continue;
        }

        
        slot.itemId = getItemIdFromStack(env, itemStack);
        if (slot.itemId < 0) {
            env->DeleteLocalRef(itemStack);
            result.push_back(slot);
            continue;
        }

        if (JniCache::fid_stackSize) {
            slot.stackSize = env->GetIntField(itemStack, JniCache::fid_stackSize);
        }

        if (JniCache::mid_getItemDamage) {
            slot.damage = env->CallIntMethod(itemStack, JniCache::mid_getItemDamage);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                
                if (JniCache::fid_itemDamage) {
                    slot.damage = env->GetIntField(itemStack, JniCache::fid_itemDamage);
                } else {
                    slot.damage = 0;
                }
            }
        } else if (JniCache::fid_itemDamage) {
            slot.damage = env->GetIntField(itemStack, JniCache::fid_itemDamage);
        }

        
        if (slot.itemId == 373) {
            
            slot.isSplashPotion = (slot.damage & 0x4000) != 0;

            
            if (slot.damage == 8197 || slot.damage == 8229 || slot.damage == 16389 || slot.damage == 16421) {
                slot.isHealthPotion = true;
            }
        }

        
        if (slot.itemId >= 298 && slot.itemId <= 317 && mid_getShort && nbtCompoundCls) {
            jclass stackCls = env->GetObjectClass(itemStack);
            jmethodID mid_getEnchantTagList = env->GetMethodID(stackCls, OBFUSCATE_STRING("getEnchantmentTagList").c_str(),
                                                               OBFUSCATE_STRING("()Lnet/minecraft/nbt/NBTTagList;").c_str());
            env->DeleteLocalRef(stackCls);

            if (mid_getEnchantTagList) {
                jobject tagList = env->CallObjectMethod(itemStack, mid_getEnchantTagList);
                if (tagList && !env->ExceptionCheck()) {
                    jclass tagListCls = env->GetObjectClass(tagList);
                    jmethodID mid_tagCount = env->GetMethodID(tagListCls, OBFUSCATE_STRING("tagCount").c_str(),
                                                              OBFUSCATE_STRING("()I").c_str());
                    jmethodID mid_getCompoundAt = env->GetMethodID(tagListCls, OBFUSCATE_STRING("getCompoundTagAt").c_str(),
                                                                   OBFUSCATE_STRING("(I)Lnet/minecraft/nbt/NBTTagCompound;").c_str());
                    env->DeleteLocalRef(tagListCls);
                    
                    if (mid_tagCount && mid_getCompoundAt) {
                        int count = env->CallIntMethod(tagList, mid_tagCount);
                        for (int e = 0; e < count; e++) {
                            jobject compound = env->CallObjectMethod(tagList, mid_getCompoundAt, e);
                            if (compound) {
                                short enchantId = env->CallShortMethod(compound, mid_getShort, strId);
                                if (enchantId == 0) {  
                                    slot.protectionLevel = env->CallShortMethod(compound, mid_getShort, strLvl);
                                    env->DeleteLocalRef(compound);
                                    break;
                                }
                                env->DeleteLocalRef(compound);
                            }
                        }
                    }
                    env->DeleteLocalRef(tagList);
                } else {
                    env->ExceptionClear();
                }
            }
        }

        env->DeleteLocalRef(itemStack);
        result.push_back(slot);
    }

    
    env->DeleteLocalRef(strId);
    env->DeleteLocalRef(strLvl);
    env->DeleteLocalRef(mainInventory);
    if (nbtCompoundCls)
        env->DeleteLocalRef(nbtCompoundCls);
    env->DeleteLocalRef(inventoryCls);
    env->DeleteLocalRef(inventory);
    env->DeleteLocalRef(localPlayer);

    return result;
}





std::vector<SlotInfo> Minecraft::getArmorSlots() {
    std::vector<SlotInfo> result;
    if (!isValid())
        return result;

    JNIEnv* env = Java::Env();
    if (!env)
        return result;

    jobject localPlayer = getPlayer();
    if (!localPlayer)
        return result;

    if (!JniCache::fid_inventory) {
        env->DeleteLocalRef(localPlayer);
        return result;
    }

    jobject inventory = env->GetObjectField(localPlayer, JniCache::fid_inventory);
    if (!inventory) {
        env->DeleteLocalRef(localPlayer);
        return result;
    }

    jclass inventoryCls = env->GetObjectClass(inventory);
    jfieldID fid_armorInventory = env->GetFieldID(inventoryCls, OBFUSCATE_STRING("armorInventory").c_str(),
                                                  OBFUSCATE_STRING("[Lnet/minecraft/item/ItemStack;").c_str());
    if (!fid_armorInventory) {
        env->ExceptionClear();
        env->DeleteLocalRef(inventoryCls);
        env->DeleteLocalRef(inventory);
        env->DeleteLocalRef(localPlayer);
        return result;
    }

    jobjectArray armorInventory = (jobjectArray)env->GetObjectField(inventory, fid_armorInventory);
    if (!armorInventory) {
        env->DeleteLocalRef(inventoryCls);
        env->DeleteLocalRef(inventory);
        env->DeleteLocalRef(localPlayer);
        return result;
    }

    
    jclass nbtCompoundCls = nullptr;
    Java::AssignClass(OBFUSCATE_STRING("net.minecraft.nbt.NBTTagCompound"), nbtCompoundCls);
    jmethodID mid_getShort = nullptr;
    if (nbtCompoundCls) {
        mid_getShort = env->GetMethodID(nbtCompoundCls, OBFUSCATE_STRING("getShort").c_str(),
                                        OBFUSCATE_STRING("(Ljava/lang/String;)S").c_str());
    }
    jstring strId = env->NewStringUTF(OBFUSCATE_STRING("id").c_str());
    jstring strLvl = env->NewStringUTF(OBFUSCATE_STRING("lvl").c_str());

    
    
    for (int slotType = 0; slotType < 4; slotType++) {
        SlotInfo slot;
        slot.slotId = slotType;

        int armorIdx = 3 - slotType;  
        jobject itemStack = env->GetObjectArrayElement(armorInventory, armorIdx);
        if (!itemStack) {
            result.push_back(slot);
            continue;
        }

        
        slot.itemId = getItemIdFromStack(env, itemStack);
        if (slot.itemId < 0) {
            env->DeleteLocalRef(itemStack);
            result.push_back(slot);
            continue;
        }

        if (JniCache::mid_getItemDamage) {
            slot.damage = env->CallIntMethod(itemStack, JniCache::mid_getItemDamage);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                slot.damage = 0;
            }
        }

        
        if (mid_getShort && nbtCompoundCls) {
            jclass stackCls = env->GetObjectClass(itemStack);
            jmethodID mid_getEnchantTagList = env->GetMethodID(stackCls, OBFUSCATE_STRING("getEnchantmentTagList").c_str(),
                                                               OBFUSCATE_STRING("()Lnet/minecraft/nbt/NBTTagList;").c_str());
            env->DeleteLocalRef(stackCls);

            if (mid_getEnchantTagList) {
                jobject tagList = env->CallObjectMethod(itemStack, mid_getEnchantTagList);
                if (tagList && !env->ExceptionCheck()) {
                    jclass tagListCls = env->GetObjectClass(tagList);
                    jmethodID mid_tagCount = env->GetMethodID(tagListCls, OBFUSCATE_STRING("tagCount").c_str(),
                                                              OBFUSCATE_STRING("()I").c_str());
                    jmethodID mid_getCompoundAt = env->GetMethodID(tagListCls, OBFUSCATE_STRING("getCompoundTagAt").c_str(),
                                                                   OBFUSCATE_STRING("(I)Lnet/minecraft/nbt/NBTTagCompound;").c_str());
                    env->DeleteLocalRef(tagListCls);

                    if (mid_tagCount && mid_getCompoundAt) {
                        int count = env->CallIntMethod(tagList, mid_tagCount);
                        for (int e = 0; e < count; e++) {
                            jobject compound = env->CallObjectMethod(tagList, mid_getCompoundAt, e);
                            if (compound) {
                                short enchantId = env->CallShortMethod(compound, mid_getShort, strId);
                                if (enchantId == 0) {
                                    slot.protectionLevel = env->CallShortMethod(compound, mid_getShort, strLvl);
                                    env->DeleteLocalRef(compound);
                                    break;
                                }
                                env->DeleteLocalRef(compound);
                            }
                        }
                    }
                    env->DeleteLocalRef(tagList);
                } else {
                    env->ExceptionClear();
                }
            }
        }

        env->DeleteLocalRef(itemStack);
        result.push_back(slot);
    }

    env->DeleteLocalRef(strId);
    env->DeleteLocalRef(strLvl);
    env->DeleteLocalRef(armorInventory);
    if (nbtCompoundCls)
        env->DeleteLocalRef(nbtCompoundCls);
    env->DeleteLocalRef(inventoryCls);
    env->DeleteLocalRef(inventory);
    env->DeleteLocalRef(localPlayer);

    return result;
}





void Minecraft::windowClick(int slotId, int button) {
    if (!isValid())
        return;

    JNIEnv* env = Java::Env();
    if (!env)
        return;

    jobject localPlayer = getPlayer();
    if (!localPlayer)
        return;

    
    if (!JniCache::fid_openContainer) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    jobject openContainer = env->GetObjectField(localPlayer, JniCache::fid_openContainer);
    if (!openContainer) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    
    int windowId = 0;
    if (JniCache::fid_windowId) {
        windowId = env->GetIntField(openContainer, JniCache::fid_windowId);
    }

    env->DeleteLocalRef(openContainer);

    
    if (!JniCache::fid_playerController) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    jobject playerController = env->GetObjectField(instance, JniCache::fid_playerController);
    if (!playerController) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    jclass controllerCls = env->GetObjectClass(playerController);

    
    jmethodID mid_windowClick = env->GetMethodID(
        controllerCls, OBFUSCATE_STRING("windowClick").c_str(),
        OBFUSCATE_STRING("(IIIILnet/minecraft/entity/player/EntityPlayer;)Lnet/minecraft/item/ItemStack;").c_str());

    if (!mid_windowClick) {
        env->DeleteLocalRef(controllerCls);
        env->DeleteLocalRef(playerController);
        env->DeleteLocalRef(localPlayer);
        return;
    }

    
    int containerSlot = slotId;
    if (slotId >= 0 && slotId <= 8) {
        containerSlot = slotId + 36;
    }

    log_debug("[windowClick] slotId=%d -> containerSlot=%d, windowId=%d, button=%d, mode=1 (shift)", slotId,
              containerSlot, windowId, button);

    
    jobject result =
        env->CallObjectMethod(playerController, mid_windowClick, windowId, containerSlot, button, 1, localPlayer);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    if (result)
        env->DeleteLocalRef(result);
    env->DeleteLocalRef(controllerCls);
    env->DeleteLocalRef(playerController);
    env->DeleteLocalRef(localPlayer);
}








void Minecraft::dropSlot(int slotId, bool wholeStack) {
    if (!isValid())
        return;

    JNIEnv* env = Java::Env();
    if (!env)
        return;

    jobject localPlayer = getPlayer();
    if (!localPlayer)
        return;

    if (!JniCache::fid_openContainer) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    jobject openContainer = env->GetObjectField(localPlayer, JniCache::fid_openContainer);
    if (!openContainer) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    int windowId = 0;
    if (JniCache::fid_windowId) {
        windowId = env->GetIntField(openContainer, JniCache::fid_windowId);
    }

    env->DeleteLocalRef(openContainer);

    if (!JniCache::fid_playerController) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    jobject playerController = env->GetObjectField(instance, JniCache::fid_playerController);
    if (!playerController) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    jclass controllerCls = env->GetObjectClass(playerController);

    
    jmethodID mid_windowClick = env->GetMethodID(
        controllerCls, OBFUSCATE_STRING("windowClick").c_str(),
        OBFUSCATE_STRING("(IIIILnet/minecraft/entity/player/EntityPlayer;)Lnet/minecraft/item/ItemStack;").c_str());

    if (!mid_windowClick) {
        env->DeleteLocalRef(controllerCls);
        env->DeleteLocalRef(playerController);
        env->DeleteLocalRef(localPlayer);
        return;
    }

    
    int containerSlot = slotId;
    if (slotId >= 0 && slotId <= 8) {
        containerSlot = slotId + 36;
    }

    const int button = wholeStack ? 1 : 0;

    log_debug("[dropSlot] slotId=%d -> containerSlot=%d, windowId=%d, button=%d, mode=4 (throw)", slotId,
              containerSlot, windowId, button);

    jobject result =
        env->CallObjectMethod(playerController, mid_windowClick, windowId, containerSlot, button, 4, localPlayer);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    if (result)
        env->DeleteLocalRef(result);
    env->DeleteLocalRef(controllerCls);
    env->DeleteLocalRef(playerController);
    env->DeleteLocalRef(localPlayer);
}






void Minecraft::windowClickSwap(int containerSlot, int hotbarIndex) {
    if (!isValid())
        return;

    JNIEnv* env = Java::Env();
    if (!env)
        return;

    jobject localPlayer = getPlayer();
    if (!localPlayer)
        return;

    
    if (!JniCache::fid_openContainer) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    jobject openContainer = env->GetObjectField(localPlayer, JniCache::fid_openContainer);
    if (!openContainer) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    
    int windowId = 0;
    if (JniCache::fid_windowId) {
        windowId = env->GetIntField(openContainer, JniCache::fid_windowId);
    }

    env->DeleteLocalRef(openContainer);

    
    if (!JniCache::fid_playerController) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    jobject playerController = env->GetObjectField(instance, JniCache::fid_playerController);
    if (!playerController) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    jclass controllerCls = env->GetObjectClass(playerController);

    
    jmethodID mid_windowClick = env->GetMethodID(
        controllerCls, OBFUSCATE_STRING("windowClick").c_str(),
        OBFUSCATE_STRING("(IIIILnet/minecraft/entity/player/EntityPlayer;)Lnet/minecraft/item/ItemStack;").c_str());

    if (!mid_windowClick) {
        env->DeleteLocalRef(controllerCls);
        env->DeleteLocalRef(playerController);
        env->DeleteLocalRef(localPlayer);
        return;
    }

    
    jobject result =
        env->CallObjectMethod(playerController, mid_windowClick, windowId, containerSlot, hotbarIndex, 2, localPlayer);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    if (result)
        env->DeleteLocalRef(result);
    env->DeleteLocalRef(controllerCls);
    env->DeleteLocalRef(playerController);
    env->DeleteLocalRef(localPlayer);
}





void Minecraft::openInventory() {
    if (!isValid())
        return;

    JNIEnv* env = Java::Env();
    if (!env)
        return;

    jobject localPlayer = getPlayer();
    if (!localPlayer)
        return;

    
    if (!JniCache::cls_GuiInventory) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    jmethodID mid_init = env->GetMethodID(JniCache::cls_GuiInventory, OBFUSCATE_STRING("<init>").c_str(),
                                          OBFUSCATE_STRING("(Lnet/minecraft/entity/player/EntityPlayer;)V").c_str());
    if (!mid_init) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    jobject guiInventory = env->NewObject(JniCache::cls_GuiInventory, mid_init, localPlayer);
    if (!guiInventory) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    
    jmethodID mid_displayGuiScreen = env->GetMethodID(cls, OBFUSCATE_STRING("displayGuiScreen").c_str(),
                                                      OBFUSCATE_STRING("(Lnet/minecraft/client/gui/GuiScreen;)V").c_str());
    if (!mid_displayGuiScreen) {
        env->ExceptionClear();
        mid_displayGuiScreen = env->GetMethodID(cls, OBFUSCATE_STRING("func_147108_a").c_str(),
                                                OBFUSCATE_STRING("(Lnet/minecraft/client/gui/GuiScreen;)V").c_str());
    }

    if (mid_displayGuiScreen) {
        env->CallVoidMethod(instance, mid_displayGuiScreen, guiInventory);
    }

    env->DeleteLocalRef(guiInventory);
    env->DeleteLocalRef(localPlayer);
}





void Minecraft::closeInventory() {
    if (!isValid())
        return;

    JNIEnv* env = Java::Env();
    if (!env)
        return;

    jobject localPlayer = getPlayer();
    if (!localPlayer)
        return;

    
    if (!JniCache::cls_EntityPlayer) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    jmethodID mid_closeScreen = env->GetMethodID(JniCache::cls_EntityPlayer, OBFUSCATE_STRING("closeScreen").c_str(),
                                                 OBFUSCATE_STRING("()V").c_str());
    if (!mid_closeScreen) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    env->CallVoidMethod(localPlayer, mid_closeScreen);
    env->DeleteLocalRef(localPlayer);
}





bool Minecraft::isInventoryOpen() {
    if (!isValid())
        return false;

    JNIEnv* env = Java::Env();
    if (!env)
        return false;

    if (!JniCache::fid_currentScreen)
        return false;

    jobject screen = env->GetObjectField(instance, JniCache::fid_currentScreen);
    if (!screen)
        return false;

    
    jclass guiContainerCls = nullptr;
    bool result = false;

    if (Java::AssignClass(OBFUSCATE_STRING("net.minecraft.client.gui.inventory.GuiContainer"), guiContainerCls)) {
        result = env->IsInstanceOf(screen, guiContainerCls);
        env->DeleteLocalRef(guiContainerCls);
    }

    env->DeleteLocalRef(screen);
    return result;
}





bool Minecraft::isPlayerInventoryOpen() {
    if (!isValid())
        return false;

    JNIEnv* env = Java::Env();
    if (!env)
        return false;

    if (!JniCache::fid_currentScreen)
        return false;

    jobject screen = env->GetObjectField(instance, JniCache::fid_currentScreen);
    if (!screen)
        return false;

    
    jclass guiInventoryCls = nullptr;
    bool result = false;

    if (Java::AssignClass(OBFUSCATE_STRING("net.minecraft.client.gui.inventory.GuiInventory"), guiInventoryCls)) {
        result = env->IsInstanceOf(screen, guiInventoryCls);
        env->DeleteLocalRef(guiInventoryCls);
    }

    env->DeleteLocalRef(screen);
    return result;
}
