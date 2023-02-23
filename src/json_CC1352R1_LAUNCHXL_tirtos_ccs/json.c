/*
 * Copyright (c) 2019-2020, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== json.c ========
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Display Header files */
#include <ti/display/Display.h>

#include <ti/utils/json/json.h>
/**************add by weli begin for json function***********************/
#define REQUEST_SCHEMA                  \
"{"                                     \
  "\"commandId\": string,"              \
  "\"command\": string,"                \
  "\"externalId\": string,"             \
  "\"password\": string"                \
"}"
#define AUTHORIZE_SCHEMA                \
"{"                                     \
  "\"commandId\": string,"              \
  "\"command\": string,"                \
  "\"originalCommand\": string,"        \
  "\"uuid\": string,"                   \
  "\"externalId\": string"              \
"}"
#define RESPONSE_SCHEMA                 \
"{"                                     \
  "\"commandId\": string,"              \
  "\"command\": string,"                \
  "\"originalCommand\": string,"        \
  "\"uuid\": string,"                   \
  "\"externalId\": string,"             \
  "\"payload\": string"                 \
"}"

#define EXAMPLE_OF_REQUEST                              \
"{"                                                     \
  "\"commandId\": \"1q2z3er56\","                       \
  "\"command\": \"OpenDoor\","                          \
  "\"externalId\": \"someidsuppliedbyapp\""             \
"}"
#define EXAMPLE_PASSWORD_OF_REQUEST                     \
"{"                                                     \
  "\"commandId\": \"1q2z3er56\","                       \
  "\"command\": \"SetBLEPass\","                        \
  "\"externalId\": \"someidsuppliedbyapp\","            \
  "\"password\": \"123456\""                            \
"}"

#define EXAMPLE_OF_AUTHORIZE                            \
"{"                                                     \
  "\"commandId\": \"1q2z3er56\","                       \
  "\"command\": \"Authorize\","                         \
  "\"originalCommand\": \"OpenDoor\","                  \
  "\"uuid\": \"112233445566\","                         \
  "\"externalId\": \"someidsuppliedbyapp\""             \
"}"

#define EXAMPLE_OF_RESPONSE                             \
"{"                                                     \
  "\"commandId\": \"1q2z3er56\","                       \
  "\"command\": \"OpenDoorReponse\","                   \
  "\"payload\": \"Rejected\""                           \
"}"
#define EXAMPLE_AUTHORIZE_OF_RESPONSE                   \
"{"                                                     \
  "\"commandId\": \"1q2z3er56\","                       \
  "\"command\": \"Authorize\","                         \
  "\"originalCommand\": \"OpenDoor\","                  \
  "\"uuid\": \"112233445566\","                         \
  "\"externalId\": \"someidsuppliedbyapp\","            \
  "\"payload\": \"Rejected\""                           \
"}"
uint8_t BJJA_LM_create_json(Json_Handle *hLocalObject,Json_Handle *hLocalTemplate,char *json_schema,char *json_data);
uint8_t BJJA_LM_json_get_token_data(Json_Handle *hLocalObject,uint8_t *ret_data,char *pKey,char *buffer);
uint8_t BJJA_LM_json_set_token_data(Json_Handle *hLocalObject,uint8_t *set_value,char *pKey,char *buffer);
uint8_t BJJA_LM_json_build(Json_Handle *hLocalObject,char *buffer,uint16_t jsonBufSize);
uint8_t BJJA_LM_destory_json(Json_Handle *hTemplate,Json_Handle *hObject);
/****************add by weli end for json function***********************/

Display_Handle display;
/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    int i;
    void *valueBuf;
    int16_t retVal;
    uint16_t valueSize;
    uint16_t jsonBufSize;

    static char jsonBuf[1024];  /* max string to hold serialized JSON buffer */
    Json_Handle hTemplate;
    Json_Handle hObject;

    Display_init();

    /* Open an available UART display using default params. */
    display = Display_open(Display_Type_UART, NULL);
    if (display == NULL) {
        /* Failed to open a display */
        while (1);
    }

    

    /***************************************************************/
    //BJJA_LM_create_json(&hObject,&hTemplate,REQUEST_SCHEMA,EXAMPLE_OF_REQUEST);
    BJJA_LM_create_json(&hObject,&hTemplate,RESPONSE_SCHEMA,EXAMPLE_OF_REQUEST);

    char token_data[128]={0x00};
    uint8_t my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"commandId\"",jsonBuf);
    if(my_ret)
    {
        Display_printf(display, 0, 0, "find the token:%s,data:%s\r\n","\"commandId\"",token_data);
    }
    memset(token_data,0x00,sizeof(token_data));
    
    my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"command\"",jsonBuf);
    if(my_ret)
    {
        Display_printf(display, 0, 0, "find the token:%s,data:%s\r\n","\"command\"",token_data);
    }

    my_ret = BJJA_LM_json_set_token_data(&hObject,"CloseDoor","\"command\"",jsonBuf);
    
    my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"command\"",jsonBuf);
    if(my_ret)
    {
        Display_printf(display, 0, 0, "find the token:%s,data:%s\r\n","\"command\"",token_data);
    }


    memset(token_data,0x00,sizeof(token_data));
    my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"externalId\"",jsonBuf);
    if(my_ret)
    {
        Display_printf(display, 0, 0, "find the token:%s,data:%s\r\n","\"externalId\"",token_data);
    }

    jsonBufSize=1024;
    BJJA_LM_json_build(&hObject,jsonBuf,jsonBufSize);

    BJJA_LM_destory_json(&hTemplate,&hObject);
    /***************************************************************/
    
    return (0);
}
uint8_t BJJA_LM_create_json(Json_Handle *hLocalObject,Json_Handle *hLocalTemplate,char *json_schema,char *json_data)
{
    int16_t retVal;
    /* create a template from a buffer containing a template */
    retVal = Json_createTemplate(hLocalTemplate, json_schema,
            strlen(json_schema));
    if (retVal != 0) {
        Display_printf(display, 0, 0, "Error creating the JSON template");
        return 0;
    }
    else {
        Display_printf(display, 0, 0, "JSON template created\n");
    }

    /* create a default-sized JSON object from the template */
    retVal = Json_createObject(hLocalObject, *hLocalTemplate, 0);
    if (retVal != 0) 
    {
        Display_printf(display, 0, 0, "Error creating JSON object");
        return 0;
    }
    else 
    {
        Display_printf(display, 0, 0, "JSON object created from template\n");
    }

    /* parse EXAMPLE_JSONBUF */
    retVal = Json_parse(*hLocalObject, json_data, strlen(json_data));
    if (retVal != 0) 
    {
        Display_printf(display, 0, 0, "Error parsing the JSON buffer");
        return 0;
    }
    else 
    {
        Display_printf(display, 0, 0, "JSON buffer parsed\n");
    }


    return 1;

}
uint8_t BJJA_LM_json_get_token_data(Json_Handle *hLocalObject,uint8_t *ret_data,char *pKey,char *buffer)
{
    void *valueBuf;
    int16_t retVal;
    uint16_t valueSize;
    uint16_t jsonBufSize;
    retVal = Json_getValue(*hLocalObject, pKey, NULL, &valueSize);
    if (retVal != 0) 
    {
        //Display_printf(display, 0, 0, "Error getting the %s buffer size",pKey);
        return 0;
    }
    else 
    {
        //Display_printf(display, 0, 0, "%s buffer size: %d\n", pKey, valueSize);
    }

    valueBuf = calloc(1, valueSize + 1);
    retVal = Json_getValue(*hLocalObject, pKey, valueBuf, &valueSize);
    if (retVal != 0) 
    {
        //Display_printf(display, 0, 0, "Error getting the %s value",pKey);
        return 0;
    }
    else 
    {
        //Display_printf(display, 0, 0, "JSON buffer parsed, %s: %s\n",pKey,valueBuf);
    }
    memcpy(ret_data,valueBuf,valueSize);
    free(valueBuf);
    return 1;

}
uint8_t BJJA_LM_json_set_token_data(Json_Handle *hLocalObject,uint8_t *set_value,char *pKey,char *buffer)
{
    int16_t retVal;
    //retVal = Json_getValue(hObject, "\"age\"", &age, &valueSize);
    retVal = Json_setValue(*hLocalObject, pKey, set_value, strlen(set_value));
    if (retVal != 0) 
    {
        Display_printf(display, 0, 0, "Error setting the age value");
        return 0;
    }
    return 1;

}
uint8_t BJJA_LM_json_build(Json_Handle *hLocalObject,char *buffer,uint16_t jsonBufSize)
{
    int16_t retVal = Json_build(*hLocalObject, buffer, &jsonBufSize);
    if (retVal != 0) 
    {
        Display_printf(display, 0, 0, "Error serializing JSON data");
        return 0;
    }
    else 
    {
        Display_printf(display, 0, 0, "serialized data:\n%s", buffer);
        return 1;
    }
}
uint8_t BJJA_LM_destory_json(Json_Handle *hTemplate,Json_Handle *hObject)
{
    int16_t retVal =0;
    /* done, cleanup */
    retVal = Json_destroyObject(*hObject);

    retVal = Json_destroyTemplate(*hTemplate);

    Display_printf(display, 0, 0, "Finished JSON example");
    return 1;
}
