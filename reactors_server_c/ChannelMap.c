#include "ChannelMap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 初始化
struct ChannelMap* channelMapInit(int size)
{
    struct ChannelMap* map = malloc(sizeof(struct ChannelMap));
    map->size = size;
    map->list = (struct Channel**)malloc(sizeof(struct Channel*)*size);
    return map;
}

// 清空map
void ChannelMapClear(struct ChannelMap* map)
{
    if(map != NULL){
        for(int i = 0; i < map->size; ++i){
            if(map->list[i] != NULL){
                free(map->list[i]);
            }
        }
        free(map->list);
        map->list = NULL;
    }
    map->size = 0;
}

// 重新分配内存空间
bool makeMapRoom(struct ChannelMap* map, int newSize, int unitSize)
{
    if(map->size < newSize){
        int curSize = map->size;
        while (curSize < newSize)
        {
            curSize *= 2;
        }
        // 扩容realloc
        struct Channel** temp = realloc(map->list,curSize * unitSize);
        if(temp == NULL){
            return false;
        }
        map->list = temp;
        memset(&map->list[map->size],0,(curSize-map->size)*unitSize);
        //memset函数：将一块内存中的内容指定为指定值
        map->size = curSize;
    }
    return true;
}