/****************************************************************************
 * Adapted from the LUFA Library:
 *
 *   Copyright 2011  Dean Camera (dean [at] fourwalledcubicle [dot] com)
 *   dean [at] fourwalledcubicle [dot] com, www.lufa-lib.org
 *
 * Permission to use, copy, modify, distribute, and sell this
 * software and its documentation for any purpose is hereby granted
 * without fee, provided that the above copyright notice appear in
 * all copies and that both that the copyright notice and this
 * permission notice and warranty disclaimer appear in supporting
 * documentation, and that the name of the author not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * The author disclaim all warranties with regard to this
 * software, including all implied warranties of merchantability
 * and fitness.  In no event shall the author be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether
 * in an action of contract, negligence or other tortious action,
 * arising out of or in connection with the use or performance of
 * this software.
 *
 ****************************************************************************/

/** \file
 *  \brief USB Human Interface Device (HID) Class report descriptor parser.
 *
 *  This file allows for the easy parsing of complex HID report descriptors, which describes the data that
 *  a HID device transmits to the host. It also provides an easy API for extracting and processing the data
 *  elements inside a HID report sent from an attached HID device.
 */

/** \ingroup Group_USB
 *  \defgroup Group_HIDParser HID Report Parser
 *  \brief USB Human Interface Device (HID) Class report descriptor parser.
 *
 *  \section Sec_HIDParser_Dependencies Module Source Dependencies
 *  The following files must be built with any user project that uses this module:
 *	- LUFA/Drivers/USB/Class/Host/HIDParser.c <i>(Makefile source module name: LUFA_SRC_USB)</i>
 *
 *  \section Sec_HIDParser_ModDescription Module Description
 *  Human Interface Device (HID) class report descriptor parser. This module implements a parser than is
 *  capable of processing a complete HID report descriptor, and outputting a flat structure containing the
 *  contents of the report in an a more friendly format. The parsed data may then be further processed and used
 *  within an application to process sent and received HID reports to and from an attached HID device.
 *
 *  A HID report descriptor consists of a set of HID report items, which describe the function and layout
 *  of data exchanged between a HID device and a host, including both the physical encoding of each item
 *  (such as a button, key press or joystick axis) in the sent and received data packets - known as "reports" -
 *  as well as other information about each item such as the usages, data range, physical location and other
 *  characteristics. In this way a HID device can retain a high degree of flexibility in its capabilities, as it
 *  is not forced to comply with a given report layout or feature-set.
 *
 *  This module also contains routines for the processing of data in an actual HID report, using the parsed report
 *  descriptor data as a guide for the encoding.
 *
 *  @{
 */

#pragma once

/* Includes: */
#include <stdint.h>
#include <stdbool.h>

/* Enable C linkage for C++ Compilers: */
#if defined(__cplusplus)
extern "C"
{
#endif

	/* Macros: */
#if !defined(HID_STATETABLE_STACK_DEPTH) || defined(__DOXYGEN__)
/** Constant indicating the maximum stack depth of the state table. A larger state table
 *  allows for more PUSH/POP report items to be nested, but consumes more memory. By default
 *  this is set to 2 levels (allowing non-nested PUSH items) but this can be overridden by
 *  defining \c HID_STATETABLE_STACK_DEPTH to another value in the user project makefile, passing the
 *  define to the compiler using the -D compiler switch.
 */
#define HID_STATETABLE_STACK_DEPTH 2
#endif

#if !defined(HID_USAGE_STACK_DEPTH) || defined(__DOXYGEN__)
/** Constant indicating the maximum stack depth of the usage table. A larger usage table
 *  allows for more USAGE items to be indicated sequentially for REPORT COUNT entries of more than
 *  one, but requires more stack space. By default this is set to 8 levels (allowing for a report
 *  item with a count of 8) but this can be overridden by defining \c HID_USAGE_STACK_DEPTH to another
 *  value in the user project makefile, passing the define to the compiler using the -D compiler
 *  switch.
 */
#define HID_USAGE_STACK_DEPTH 8
#endif

/** Returns the value a given HID report item (once its value has been fetched via \ref USB_GetHIDReportItemInfo())
 *  left-aligned to the given data type. This allows for signed data to be interpreted correctly, by shifting the data
 *  leftwards until the data's sign bit is in the correct position.
 *
 *  \param[in] ReportItem  HID Report Item whose retrieved value is to be aligned.
 *  \param[in] Type		Data type to align the HID report item's value to.
 *
 *  \return Left-aligned data of the given report item's pre-retrieved value for the given datatype.
 */
#define HID_ALIGN_DATA(ReportItem, Type) ((Type)(ReportItem->Value << ((8 * sizeof(Type)) - ReportItem->Attributes.BitSize)))

/** Convenience macro to determine the larger of two values.
 *
 *  \attention This macro should only be used with operands that do not have side effects from being evaluated
 *			 multiple times.
 *
 *  \param[in] x  First value to compare
 *  \param[in] y  First value to compare
 *
 *  \return The larger of the two input parameters
 */
#if !defined(MAX) || defined(__DOXYGEN__)
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

/** Convenience macro to determine the smaller of two values.
 *
 *  \attention This macro should only be used with operands that do not have side effects from being evaluated
 *			 multiple times.
 *
 *  \param[in] x  First value to compare.
 *  \param[in] y  First value to compare.
 *
 *  \return The smaller of the two input parameters
 */
#if !defined(MIN) || defined(__DOXYGEN__)
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#if !defined(CONCAT) || defined(__DOXYGEN__)
/** Concatenates the given input into a single token, via the C Preprocessor.
 *
 *  \param[in] x  First item to concatenate.
 *  \param[in] y  Second item to concatenate.
 *
 *  \return Concatenated version of the input.
 */
#define CONCAT(x, y) x##y

/** CConcatenates the given input into a single token after macro expansion, via the C Preprocessor.
 *
 *  \param[in] x  First item to concatenate.
 *  \param[in] y  Second item to concatenate.
 *
 *  \return Concatenated version of the expanded input.
 */
#define CONCAT_EXPANDED(x, y) CONCAT(x, y)
#endif

	/* Public Interface - May be used in end-application: */
	/* Enums: */
	/** Enum for the possible error codes in the return value of the \ref USB_ProcessHIDReport() function. */
	enum HID_Parse_ErrorCodes_t
	{
		HID_PARSE_Successful = 0,				   /**< Successful parse of the HID report descriptor, no error. */
		HID_PARSE_HIDStackOverflow = 1,			   /**< More than \ref HID_STATETABLE_STACK_DEPTH nested PUSHes in the report. */
		HID_PARSE_HIDStackUnderflow = 2,		   /**< A POP was found when the state table stack was empty. */
		HID_PARSE_UnexpectedEndCollection = 3,	   /**< An END COLLECTION item found without matching COLLECTION item. */
		HID_PARSE_UsageListOverflow = 4,		   /**< More than \ref HID_USAGE_STACK_DEPTH usages listed in a row. */
		HID_PARSE_NoUnfilteredReportItems = 5,	   /**< All report items from the device were filtered by the filtering callback routine. */
	};

	/* Private Interface - For use in library only: */
#if !defined(__DOXYGEN__)
	/* Macros: */
#define HID_RI_DATA_SIZE_MASK 0x03
#define HID_RI_TYPE_MASK 0x0C
#define HID_RI_TAG_MASK 0xF0

#define HID_RI_TYPE_MAIN 0x00
#define HID_RI_TYPE_GLOBAL 0x04
#define HID_RI_TYPE_LOCAL 0x08

#define HID_RI_DATA_BITS_0 0x00
#define HID_RI_DATA_BITS_8 0x01
#define HID_RI_DATA_BITS_16 0x02
#define HID_RI_DATA_BITS_32 0x03
#define HID_RI_DATA_BITS(DataBits) CONCAT_EXPANDED(HID_RI_DATA_BITS_, DataBits)

#define _HID_RI_ENCODE_0(Data)
#define _HID_RI_ENCODE_8(Data) , (Data & 0xFF)
#define _HID_RI_ENCODE_16(Data) \
	_HID_RI_ENCODE_8(Data)	  \
	_HID_RI_ENCODE_8(Data >> 8)
#define _HID_RI_ENCODE_32(Data) \
	_HID_RI_ENCODE_16(Data)	 \
	_HID_RI_ENCODE_16(Data >> 16)
#define _HID_RI_ENCODE(DataBits, ...) CONCAT_EXPANDED(_HID_RI_ENCODE_, DataBits(__VA_ARGS__))

#define _HID_RI_ENTRY(Type, Tag, DataBits, ...) (Type | Tag | HID_RI_DATA_BITS(DataBits)) _HID_RI_ENCODE(DataBits, (__VA_ARGS__))
#endif

	/* Public Interface - May be used in end-application: */
	/* Macros: */
	/** \name HID Input, Output and Feature Report Descriptor Item Flags */
	/**@{*/
#define HID_IOF_CONSTANT (1 << 0)
#define HID_IOF_DATA (0 << 0)
#define HID_IOF_VARIABLE (1 << 1)
#define HID_IOF_ARRAY (0 << 1)
#define HID_IOF_RELATIVE (1 << 2)
#define HID_IOF_ABSOLUTE (0 << 2)
#define HID_IOF_WRAP (1 << 3)
#define HID_IOF_NO_WRAP (0 << 3)
#define HID_IOF_NON_LINEAR (1 << 4)
#define HID_IOF_LINEAR (0 << 4)
#define HID_IOF_NO_PREFERRED_STATE (1 << 5)
#define HID_IOF_PREFERRED_STATE (0 << 5)
#define HID_IOF_NULLSTATE (1 << 6)
#define HID_IOF_NO_NULL_POSITION (0 << 6)
#define HID_IOF_VOLATILE (1 << 7)
#define HID_IOF_NON_VOLATILE (0 << 7)
#define HID_IOF_BUFFERED_BYTES (1 << 8)
#define HID_IOF_BITFIELD (0 << 8)
	/**@}*/

	/** \name HID Report Descriptor Item Macros */
	/**@{*/
#define HID_RI_INPUT(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_MAIN, 0x80, DataBits, __VA_ARGS__)
#define HID_RI_OUTPUT(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_MAIN, 0x90, DataBits, __VA_ARGS__)
#define HID_RI_COLLECTION(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_MAIN, 0xA0, DataBits, __VA_ARGS__)
#define HID_RI_FEATURE(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_MAIN, 0xB0, DataBits, __VA_ARGS__)
#define HID_RI_END_COLLECTION(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_MAIN, 0xC0, DataBits, __VA_ARGS__)
#define HID_RI_USAGE_PAGE(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x00, DataBits, __VA_ARGS__)
#define HID_RI_LOGICAL_MINIMUM(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x10, DataBits, __VA_ARGS__)
#define HID_RI_LOGICAL_MAXIMUM(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x20, DataBits, __VA_ARGS__)
#define HID_RI_PHYSICAL_MINIMUM(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x30, DataBits, __VA_ARGS__)
#define HID_RI_PHYSICAL_MAXIMUM(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x40, DataBits, __VA_ARGS__)
#define HID_RI_UNIT_EXPONENT(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x50, DataBits, __VA_ARGS__)
#define HID_RI_UNIT(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x60, DataBits, __VA_ARGS__)
#define HID_RI_REPORT_SIZE(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x70, DataBits, __VA_ARGS__)
#define HID_RI_REPORT_ID(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x80, DataBits, __VA_ARGS__)
#define HID_RI_REPORT_COUNT(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x90, DataBits, __VA_ARGS__)
#define HID_RI_PUSH(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0xA0, DataBits, __VA_ARGS__)
#define HID_RI_POP(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0xB0, DataBits, __VA_ARGS__)
#define HID_RI_USAGE(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_LOCAL, 0x00, DataBits, __VA_ARGS__)
#define HID_RI_USAGE_MINIMUM(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_LOCAL, 0x10, DataBits, __VA_ARGS__)
#define HID_RI_USAGE_MAXIMUM(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_LOCAL, 0x20, DataBits, __VA_ARGS__)
	/**@}*/

	/* Type Defines: */
	/** \brief HID Parser Report Item Min/Max Structure.
	 *
	 *  Type define for an attribute with both minimum and maximum values (e.g. Logical Min/Max).
	 */
	typedef struct
	{
		uint32_t Minimum; /**< Minimum value for the attribute. */
		uint32_t Maximum; /**< Maximum value for the attribute. */
	} HID_MinMax_t;

	/** \brief HID Parser Report Item Unit Structure.
	 *
	 *  Type define for the Unit attributes of a report item.
	 */
	typedef struct
	{
		uint32_t Type;	  /**< Unit type (refer to HID specifications for details). */
		uint8_t Exponent; /**< Unit exponent (refer to HID specifications for details). */
	} HID_Unit_t;

	/** \brief HID Parser Report Item Usage Structure.
	 *
	 *  Type define for the Usage attributes of a report item.
	 */
	typedef struct
	{
		uint16_t Page;	/**< Usage page of the report item. */
		uint16_t Usage; /**< Usage of the report item. */
	} HID_Usage_t;

	/** \brief HID Parser Report Item Collection Path Structure.
	 *
	 *  Type define for a COLLECTION object. Contains the collection attributes and a reference to the
	 *  parent collection if any.
	 */
	typedef struct HID_CollectionPath
	{
		uint8_t Type;					   /**< Collection type (e.g. "Generic Desktop"). */
		HID_Usage_t Usage;				   /**< Collection usage. */
		struct HID_CollectionPath *Parent; /**< Reference to parent collection, or \c NULL if root collection. */
		struct HID_CollectionPath *Next; /**< Reference to parent collection, or \c NULL if root collection. */
	} HID_CollectionPath_t;

	/** \brief HID Parser Report Item Attributes Structure.
	 *
	 *  Type define for all the data attributes of a report item, except flags.
	 */
	typedef struct
	{
		uint8_t BitSize; /**< Size in bits of the report item's data. */
		HID_Usage_t Usage;	   /**< Usage of the report item. */
		HID_Unit_t Unit;	   /**< Unit type and exponent of the report item. */
		HID_MinMax_t Logical;  /**< Logical minimum and maximum of the report item. */
		HID_MinMax_t Physical; /**< Physical minimum and maximum of the report item. */
	} HID_ReportItem_Attributes_t;

	/** \brief HID Parser Report Item Details Structure.
	 *
	 *  Type define for a report item (IN, OUT or FEATURE) layout attributes and other details.
	 */
	typedef struct HID_ReportItem_s
	{
		uint16_t BitOffset;					  /**< Bit offset in the IN, OUT or FEATURE report of the item. */
		uint8_t ItemType;					  /**< Report item type, a value in \ref HID_ReportItemTypes_t. */
		uint16_t ItemFlags;					  /**< Item data flags, a mask of \c HID_IOF_* constants. */
		uint8_t ReportID;					  /**< Report ID this item belongs to, or 0x00 if device has only one report */
		HID_ReportItem_Attributes_t Attributes; /**< Report item attributes. */
		uint8_t ReportCount;
		uint32_t Value;			/**< Current value of the report item - use \ref HID_ALIGN_DATA() when processing
								 *   a retrieved value so that it is aligned to a specific type.
								 */
		uint32_t PreviousValue; /**< Previous value of the report item. */
		struct HID_ReportItem_s* Next;
	} HID_ReportItem_t;

	/** \brief HID Parser Report Size Structure.
	 *
	 *  Type define for a report item size information structure, to retain the size of a device's reports by ID.
	 */
	typedef struct HID_ReportSizeInfo_s
	{
		uint8_t ReportID;			/**< Report ID of the report within the HID interface. */
		uint16_t ReportSizeBits[3]; /**< Total number of bits in each report type for the given Report ID,
									 *   indexed by the \ref HID_ReportItemTypes_t enum.
									 */
		struct HID_ReportSizeInfo_s* Next;
	} HID_ReportSizeInfo_t;

	/** \brief HID Parser State Structure.
	 *
	 *  Type define for a complete processed HID report, including all report item data and collections.
	 */
	
	typedef struct 
	{
		uint8_t TotalReportItems;								   /**< Total number of report items stored in the \c ReportItems array. */
		HID_ReportItem_t* FirstReportItem;		   /**< Report items array, including all IN, OUT
																	*   and FEATURE items.
																	*/
		uint8_t TotalDeviceReports;								   /**< Number of reports within the HID interface */
		uint16_t LargestReportSizeBits;							   /**< Largest report that the attached device will generate, in bits */
		bool UsingReportIDs;									   /**< Indicates if the device has at least one REPORT ID
																	*   element in its HID report descriptor.
																	*/
		HID_ReportItem_t* LastReportItem;
	} HID_ReportInfo_t;

	/* Function Prototypes: */
	/** Function to process a given HID report returned from an attached device, and store it into a given
	 *  \ref HID_ReportInfo_t structure.
	 *
	 *  \param[in]  ReportData  Buffer containing the device's HID report table.
	 *  \param[in]  ReportSize  Size in bytes of the HID report table.
	 *  \param[out] ParserData  Pointer to a \ref HID_ReportInfo_t instance for the parser output.
	 *
	 *  \return A value in the \ref HID_Parse_ErrorCodes_t enum.
	 */
	uint8_t USB_ProcessHIDReport(uint8_t dev_addr,
								 uint8_t instance,
								 const uint8_t *ReportData,
								 uint16_t ReportSize,
								 HID_ReportInfo_t **ParserData);

	void USB_FreeReportInfo(HID_ReportInfo_t *ReportInfo);

	/** Extracts the given report item's value out of the given HID report and places it into the Value
	 *  member of the report item's \ref HID_ReportItem_t structure.
	 *
	 *  When called on a report with an item that exists in that report, this copies the report item's \c Value
	 *  to its \c PreviousValue element for easy checking to see if an item's value has changed before processing
	 *  a report. If the given item does not exist in the report, the function does not modify the report item's
	 *  data.
	 *
	 *  \param[in]	 ReportData  Buffer containing an IN or FEATURE report from an attached device.
	 *  \param[in,out] ReportItem  Pointer to the report item of interest in a \ref HID_ReportInfo_t ReportItem array.
	 *
	 *  \returns Boolean \c true if the item to retrieve was located in the given report, \c false otherwise.
	 */
	bool USB_GetHIDReportItemInfo(uint16_t report_id, const uint8_t *ReportData,
								  HID_ReportItem_t *const ReportItem);

	/** Callback routine for the HID Report Parser. This callback <b>must</b> be implemented by the user code when
	 *  the parser is used, to determine what report IN, OUT and FEATURE item's information is stored into the user
	 *  \ref HID_ReportInfo_t structure. This can be used to filter only those items the application will be using, so that
	 *  no RAM is wasted storing the attributes for report items which will never be referenced by the application.
	 *
	 *  Report item pointers passed to this callback function may be cached by the user application for later use
	 *  when processing report items. This provides faster report processing in the user application than would
	 *  a search of the entire parsed report item table for each received or sent report.
	 *
	 *  \param[in] CurrentItem  Pointer to the current report item for user checking.
	 *
	 *  \return Boolean \c true if the item should be stored into the \ref HID_ReportInfo_t structure, \c false if
	 *		  it should be ignored.
	 */
	bool CALLBACK_HIDParser_FilterHIDReportItem(uint8_t dev_addr, uint8_t instance, HID_ReportItem_t *const CurrentItem);

	/** Enum for the different types of HID reports. */
	enum HID_ReportItemTypes_t
	{
		HID_REPORT_ITEM_In = 0,		 /**< Indicates that the item is an IN report type. */
		HID_REPORT_ITEM_Out = 1,	 /**< Indicates that the item is an OUT report type. */
		HID_REPORT_ITEM_Feature = 2, /**< Indicates that the item is a FEATURE report type. */
	};

/* Private Interface - For use in library only: */
#if !defined(__DOXYGEN__)
	/* Type Defines: */
	typedef struct
	{
		HID_ReportItem_Attributes_t Attributes;
		uint8_t ReportCount;
		uint8_t ReportID;
	} HID_StateTable_t;
#endif

	/* Disable C linkage for C++ Compilers: */
#if defined(__cplusplus)
}
#endif

/** @} */