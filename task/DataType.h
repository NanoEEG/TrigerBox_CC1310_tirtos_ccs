#ifndef DATATYPE_H_
#define DATATYPE_H_


/*******************************************************************
 * TYPEDEFS
 */

/*!
    \brief  I2C传输数据结构
 */
#pragma pack(push)
#pragma pack(1)

typedef struct
{
    uint8_t     Type;           //!< 标签类型
}Buff_t;


#endif /* DATATYPE_H_ */
