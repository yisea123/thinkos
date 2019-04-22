/* 
 * File:	 usb_comm-quueue.c
 * Author:   Robinson Mittmann (bobmittmann@gmail.com)
 * Target:
 * Comment:
 * Copyright(C) 2011 Bob Mittmann. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <sys/stm32f.h>
#include <arch/cortex-m3.h>
#include <sys/param.h>
#include <sys/serial.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <vt100.h>

#include <sys/usb-dev.h>
#include <sys/usb-cdc.h>

#include <sys/dcclog.h>

#define __THINKOS_DBGMON__
#include <thinkos/dbgmon.h>
#define __THINKOS_QUEUE__
#include <thinkos/queue.h>

#if (THINKOS_ENABLE_MONITOR)

#ifndef THINKOS_DBGMON_ENABLE_FLOWCTL
#define THINKOS_DBGMON_ENABLE_FLOWCTL 0
#endif

#if THINKOS_DBGMON_ENABLE_COMM_STATS
#define THINKOS_DBGMON_ENABLE_COMM_STATS 0
#endif

#define EP0_ADDR 0
#define EP0_MAX_PKT_SIZE 64

#define EP_OUT0_ADDR 1
#define EP_IN0_ADDR  2
#define EP_INT0_ADDR 3


#ifndef CDC_EP_OUT_MAX_PKT_SIZE
#define CDC_EP_OUT_MAX_PKT_SIZE 64
#endif

#ifndef CDC_EP_IN_MAX_PKT_SIZE 
#define CDC_EP_IN_MAX_PKT_SIZE 64
#endif

#ifndef CDC_EP_INT_MAX_PKT_SIZE 
#define CDC_EP_INT_MAX_PKT_SIZE 64
#endif

struct cdc_acm_descriptor_config {
	struct usb_descriptor_configuration cfg;
	struct usb_descriptor_interface comm_if;
	struct cdc_descriptor_header hdr;
	struct cdc_descriptor_call_management cm;
	struct cdc_descriptor_abstract_control_management acm;
	struct cdc_descriptor_union_1slave un;
	struct usb_descriptor_endpoint ep_int;
	struct usb_descriptor_interface if_data;
	struct usb_descriptor_endpoint ep_out;
	struct usb_descriptor_endpoint ep_in;
} __attribute__((__packed__));

struct cdc_acm_descriptor_set {
	struct usb_descriptor_configuration cfg;
	struct usb_descriptor_interface if0;
	struct cdc_descriptor_header hdr0;
	struct cdc_descriptor_call_management cm0;
	struct cdc_descriptor_abstract_control_management acm0;
	struct cdc_descriptor_union_1slave un0;
	struct usb_descriptor_endpoint ep_int0;
	struct usb_descriptor_interface if_data0;
	struct usb_descriptor_endpoint ep_out0;
	struct usb_descriptor_endpoint ep_in0;
} __attribute__((__packed__));

#define ATMEL_VCOM_PRODUCT_ID 0x6119
#define ST_VCOM_PRODUCT_ID    0x5740

#ifndef CDC_ACM_PRODUCT_ID
#define CDC_ACM_PRODUCT_ID ST_VCOM_PRODUCT_ID 
#endif

static const struct usb_descriptor_device cdc_acm_desc_dev = {
	/* Size of this descriptor in bytes */
	.length = sizeof(struct usb_descriptor_device),
	/* DEVICE descriptor type */
	.type = USB_DESCRIPTOR_DEVICE,
	/* USB specification release number */
	.usb_release = USB2_00,
	/* Class code */
	.dev_class = USB_CLASS_COMMUNICATION,
	/* Subclass code */
	.dev_subclass = 0x00,
	/* Protocol code */
	.dev_proto = 0x00,
	/* Control endpoint 0 max. packet size */
	.max_pkt_sz0 = EP0_MAX_PKT_SIZE,
	/* Vendor ID */
	.vendor_id = USB_VENDOR_ST,
	/* Product ID */
	.product_id = CDC_ACM_PRODUCT_ID,
	/* Device release number */
	.dev_release = 2,
	/* Index of manufacturer string descriptor */
	.manufacturer = 1,
	/* Index of product string descriptor */
	.product = 2,
	/* Index of S.N.  string descriptor */
	.serial_num = 3,
	/* Number of possible configurations */
	.num_of_conf = 1
};

/* device qualifier structure provide information on a high-speed
   capable device if the device was operating at the other speed.
   see usb_20.pdf - Section 9.6.2 */
const struct usb_descriptor_device_qualifier cdc_acm_desc_qual = {
	/* Size of this descriptor in bytes */
	.length = sizeof(struct usb_descriptor_device_qualifier),
	/* DEVICE_QUALIFIER descriptor type */
	.descriptortype = USB_DESCRIPTOR_DEVICE_QUALIFIER,
	/* USB specification release number */
	.usb = USB2_00,
	/* Class code */
	.deviceclass = CDC_DEVICE_CLASS,
	/* Sub-class code */
	.devicesubclass = CDC_DEVICE_SUBCLASS,
	/* Protocol code */
	.deviceprotocol = CDC_DEVICE_PROTOCOL,
	/* Control endpoint 0 max. packet size */
	.maxpacketsize0 = 0,
	/* Number of possible configurations */
	.numconfigurations = 0,
	/* Reserved for future use, must be 0 */
	.reserved = 0 
};

#define CDC_IF0 0

/* Configuration 1 descriptor */
static const struct cdc_acm_descriptor_set cdc_acm_desc_cfg = {
	.cfg = {
		.length = sizeof(struct usb_descriptor_configuration),
		.type = USB_DESCRIPTOR_CONFIGURATION,
		.total_length = sizeof(struct cdc_acm_descriptor_set),
		.num_interfaces = 2,
		.configuration_value = 1,
		.configuration = 0,
		.attributes = USB_CONFIG_SELF_NOWAKEUP,
		.max_power = USB_POWER_MA(250)
	},
	/* CDC 0 */
	/* Communication Class Interface Descriptor */
	.if0 = {
		.length = sizeof(struct usb_descriptor_interface),
		.type = USB_DESCRIPTOR_INTERFACE,
		.number = CDC_IF0,
		.alt_setting = 0,
		.num_endpoints = 1,
		.ceclass = CDC_INTERFACE_CLASS_COMMUNICATION,
		.esubclass = CDC_ABSTRACT_CONTROL_MODEL,
		.protocol = CDC_PROTOCOL_NONE,
		.interface = 4
	},
	/* Header Functional Descriptor */
	.hdr0 = {
		.bFunctionLength = sizeof(struct cdc_descriptor_header),
		.bDescriptorType = CDC_CS_INTERFACE,
		.bDescriptorSubtype = CDC_HEADER,
		.bcdCDC = CDC1_10
	},
	/* Call Management Functional Descriptor */
	.cm0 = {
		.bFunctionLength = sizeof(struct cdc_descriptor_call_management),
		.bDescriptorType = CDC_CS_INTERFACE,
		.bDescriptorSubtype = CDC_CALL_MANAGEMENT,
		.bmCapabilities = CDC_CALL_MANAGEMENT_SELF,
		/* interface used for call management */
		.bDataInterface = CDC_IF0 + 1
	},
	/* Abstract Control Management Functional Descriptor */
	.acm0 = {
		.bFunctionLength = 
			sizeof(struct cdc_descriptor_abstract_control_management),
		.bDescriptorType = CDC_CS_INTERFACE,
		.bDescriptorSubtype = CDC_ABSTRACT_CONTROL_MANAGEMENT,
		.bmCapabilities = CDC_ACM_COMMFEATURE + CDC_ACM_LINE + 
			CDC_ACM_SENDBREAK
	},
	/* Union Functional Descriptor */
	.un0 = {
		.bFunctionLength = sizeof(struct cdc_descriptor_union_1slave),
		.bDescriptorType = CDC_CS_INTERFACE,
		.bDescriptorSubtype = CDC_UNION,
		.bMasterInterface = CDC_IF0,
		.bSlaveInterface = CDC_IF0 + 1
	},
	/* Endpoint 3 descriptor */
	.ep_int0 = {
		.length = sizeof(struct usb_descriptor_endpoint),
		.type = USB_DESCRIPTOR_ENDPOINT,
		.endpointaddress= USB_ENDPOINT_IN + EP_INT0_ADDR,
		.attributes = ENDPOINT_TYPE_INTERRUPT,
		.maxpacketsize = CDC_EP_INT_MAX_PKT_SIZE,
		.interval = 200
	},
	/* Data Class Interface Descriptor Requirement */
	.if_data0 = {
		.length = sizeof(struct usb_descriptor_interface),
		.type = USB_DESCRIPTOR_INTERFACE,
		.number = CDC_IF0 + 1,
		.alt_setting = 0,
		.num_endpoints = 2,
		.ceclass =  CDC_INTERFACE_CLASS_DATA,
		.esubclass = CDC_INTERFACE_SUBCLASS_DATA,
		.protocol = CDC_PROTOCOL_NONE,
		.interface = CDC_IF0
	},
	/* First alternate setting */
	/* Endpoint 1 descriptor */
	.ep_out0 = {
		.length = sizeof(struct usb_descriptor_endpoint),
		.type = USB_DESCRIPTOR_ENDPOINT,
		.endpointaddress = USB_ENDPOINT_OUT + EP_OUT0_ADDR,
		.attributes = ENDPOINT_TYPE_BULK,
		.maxpacketsize = CDC_EP_OUT_MAX_PKT_SIZE,
		.interval = 0
	},
	/* Endpoint 2 descriptor */
	.ep_in0 = {
		.length = sizeof(struct usb_descriptor_endpoint),
		.type = USB_DESCRIPTOR_ENDPOINT,
		.endpointaddress= USB_ENDPOINT_IN + EP_IN0_ADDR,
		.attributes = ENDPOINT_TYPE_BULK,
		.maxpacketsize = CDC_EP_IN_MAX_PKT_SIZE,
		.interval = 0
	},

};


#define CDC_STOP_BITS_1   (0 << 0)
#define CDC_STOP_BITS_1_5 (1 << 0)
#define CDC_STOP_BITS_2   (2 << 0)
#define CDC_STOP_BITS_ERR (3 << 0)
#define CDC_CHAR_FORMAT_SET(VAL)  (((VAL) & 0x3) << 0)
#define CDC_CHAR_FORMAT_GET(VAL)  (((VAL) >> 0) & 0x3)
#define CDC_STOP_BITS_MAX 2

#define CDC_PARITY_NONE   (0 << 2)
#define CDC_PARITY_ODD    (1 << 2)
#define CDC_PARITY_EVEN   (2 << 2)
#define CDC_PARITY_MARK   (3 << 2)
#define CDC_PARITY_SPACE  (4 << 2)
#define CDC_PARITY_ERR    (7 << 2)
#define CDC_PARITY_SET(VAL)  (((VAL) & 0x7) << 2)
#define CDC_PARITY_GET(VAL)  (((VAL) >> 2) & 0x7)
#define CDC_PARITY_MAX    4

#define CDC_DATA_BITS_5   (0 << 5)
#define CDC_DATA_BITS_6   (1 << 5)
#define CDC_DATA_BITS_7   (2 << 5)
#define CDC_DATA_BITS_8   (3 << 5)
#define CDC_DATA_BITS_16  (4 << 5)
#define CDC_DATA_BITS_ERR (7 << 5)
#define CDC_DATA_BITS_SET(VAL)  data_bit_lut[(VAL) & 0x1f]
#define CDC_DATA_BITS_GET(VAL)  data_bit_rev_lut[((VAL) >> 5) & 0x7]
#define CDC_DATA_BITS_MAX 16


//"USB Serial (CDC) Generic Device"

extern const struct usb_descriptor_string language_english_us;

static const struct usb_descriptor_string * const cdc_acm_str[] = {
	&language_english_us,
//	&stmicro_str,
//	&thinkos_str,
//	&serial_num_str,
//	&st_vcom_str 
};

#define USB_STRCNT() (sizeof(cdc_acm_str) / sizeof(uint8_t *))

static const struct cdc_line_coding usb_cdc_lc = {
    .dwDTERate = 38400,
    .bCharFormat = 0,
    .bParityType = 0,
    .bDataBits = 8
};

static const uint16_t device_status = USB_DEVICE_STATUS_SELF_POWERED;
static const uint16_t interface_status = 0x0000;


#define ACM_USB_SUSPENDED (1 << 1)
#define ACM_CONNECTED     (1 << 2)

#define CDC_CTL_BUF_LEN 16

struct usb_cdc_acm_dev {
	/* underling USB device */
	struct usb_dev * usb;
	/* endpoints handlers */
	uint8_t ctl_ep;
	uint8_t in_ep;
	uint8_t out_ep;
	uint8_t int_ep;

	uint8_t configured;
	uint32_t ctl_buf[CDC_CTL_BUF_LEN / 4];

	volatile uint8_t acm_ctrl; /* modem control lines */

	volatile uint32_t tx_seq; 
	volatile uint32_t tx_ack; 

	volatile uint32_t rx_seq; 
	volatile uint32_t rx_ack; 
	uint8_t rx_paused;
	volatile uint8_t rx_cnt; 
	volatile uint8_t rx_pos; 
	uint8_t rx_buf[CDC_EP_IN_MAX_PKT_SIZE];

#if THINKOS_DBGMON_ENABLE_COMM_STATS
	struct {
		uint32_t tx_pkt;
		uint32_t rx_pkt;
	} stats;
#endif

};

struct usb_class_if {
	struct usb_cdc_acm_dev dev;
};


static void usb_mon_on_rcv(usb_class_t * cl, 
						   unsigned int ep_id, unsigned int len)
{
	struct usb_cdc_acm_dev * dev = (struct usb_cdc_acm_dev *)cl;
	uint32_t seq;
	uint32_t ack;
	unsigned int free;
	unsigned int pos;
	unsigned int cnt;
	unsigned int n;

	seq = dev->rx_seq;
	ack = dev->rx_ack;
	free = CDC_EP_IN_MAX_PKT_SIZE - (int32_t)(seq - ack);
	if (len > free) {
		DCC_LOG3(LOG_WARNING, "len=%d free=%d drop=%d", len, free, len - free);
		cnt = free;
	} else {
		cnt = len;
	}
	pos = seq % CDC_EP_IN_MAX_PKT_SIZE;
	if (pos == 0) {
		n = usb_dev_ep_pkt_recv(dev->usb, dev->out_ep, dev->rx_buf, cnt);

		DCC_LOG4(LOG_TRACE, "1. seq=%d ack=%d cnt=%d n=%d", 
				 seq, ack, cnt, n);

		seq += n;
	} else {
		unsigned int m;

		m = MIN((CDC_EP_IN_MAX_PKT_SIZE - pos), cnt);
		n = usb_dev_ep_pkt_recv(dev->usb, dev->out_ep, 
								&dev->rx_buf[pos],  m);
		m = cnt - n;
		m = usb_dev_ep_pkt_recv(dev->usb, dev->out_ep, 
								dev->rx_buf, m);
		DCC_LOG5(LOG_TRACE, "2. seq=%d ack=%d cnt=%d n=%d m=%d", 
				 seq, ack, cnt, n, m);
		seq += m + n;
	}
	dev->rx_seq = seq;

	usb_dev_ep_ctl(dev->usb, ep_id, USB_EP_RECV_OK);

	if ((seq - (ack = dev->rx_ack)) > 0) {
		DCC_LOG2(LOG_INFO, "COMM_TRACE seq=%d ack=%d", seq, ack);
		dbgmon_signal(DBGMON_COMM_RCV);
	}
}

static void usb_mon_on_eot(usb_class_t * cl, unsigned int ep_id)
{
//	struct usb_cdc_acm_dev * dev = &cl->dev;
	dbgmon_signal(DBGMON_COMM_EOT);
	DCC_LOG(LOG_INFO, "COMM_EOT");
}

static void usb_mon_on_eot_int(usb_class_t * cl, unsigned int ep_id)
{
	DCC_LOG1(LOG_MSG, "ep_id=%d", ep_id);
}

static const usb_dev_ep_info_t usb_mon_in_info = {
	.addr = USB_ENDPOINT_IN + EP_IN0_ADDR,
	.attr = ENDPOINT_TYPE_BULK,
	.mxpktsz = CDC_EP_IN_MAX_PKT_SIZE,
	.on_in = usb_mon_on_eot
};

static const usb_dev_ep_info_t usb_mon_out_info = {
	.addr = USB_ENDPOINT_OUT + EP_OUT0_ADDR,
	.attr = ENDPOINT_TYPE_BULK,
	.mxpktsz = CDC_EP_OUT_MAX_PKT_SIZE,
	.on_out = usb_mon_on_rcv
};

static const usb_dev_ep_info_t usb_mon_int_info = {
	.addr = USB_ENDPOINT_IN + EP_INT0_ADDR,
	.attr = ENDPOINT_TYPE_INTERRUPT,
	.mxpktsz = CDC_EP_INT_MAX_PKT_SIZE,
	.on_in = usb_mon_on_eot_int
};


/* Setup requests callback handler */
static int usb_mon_on_setup(usb_class_t * cl, 
							struct usb_request * req, void ** ptr) 
{
	struct usb_cdc_acm_dev * dev = &cl->dev;
	struct usb_dev * usb = dev->usb;
	int value = req->value;
	int index = req->index;
	int len = 0;
	int desc;

	(void)index;


	/* Handle supported standard device request Cf
	 Table 9-3 in USB specification Rev 1.1 */

	switch ((req->request << 8) | req->type) {

	/* Standard Device Requests */
		
	case STD_GET_DESCRIPTOR:
		desc = value >> 8;

		if (desc == USB_DESCRIPTOR_DEVICE) {
			/* Return Device Descriptor */
			*ptr = (void *)&cdc_acm_desc_dev;
			len = sizeof(struct usb_descriptor_device);
			DCC_LOG1(LOG_TRACE, "GetDesc: Device: len=%d", len);
			break;
		}
#if 0
		if (desc == USB_DESCRIPTOR_DEVICE_QUALIFIER) {
			/* Return Device Descriptor */
			*ptr = (void *)&cdc_acm_desc_qual;
			len = sizeof(struct usb_descriptor_device_qualifier);
			DCC_LOG1(LOG_TRACE, "GetDesc: Device: len=%d", len);
			break;
		}
#endif
		if (desc == USB_DESCRIPTOR_CONFIGURATION) {
			/* Return Configuration Descriptor */
			*ptr = (void *)&cdc_acm_desc_cfg;
			len = sizeof(struct cdc_acm_descriptor_set);
			DCC_LOG1(LOG_TRACE, "GetDesc: Config: len=%d", len);
			break;
		}

		if (desc == USB_DESCRIPTOR_STRING) {
			unsigned int n = value & 0xff;
			DCC_LOG1(LOG_TRACE, "GetDesc: String[%d]", n);
			if (n < USB_STRCNT()) {
				*ptr = (void *)cdc_acm_str[n];
				len = cdc_acm_str[n]->length;
			}
			break;
		}
		len = 0;
		DCC_LOG1(LOG_TRACE, "GetDesc: %d ?", desc);
		break;

	case STD_SET_ADDRESS:
		DCC_LOG1(LOG_TRACE, "SetAddr: %d [ADDRESS]", value);
		break;

	case STD_SET_CONFIGURATION: {
		DCC_LOG1(LOG_TRACE, "SetCfg: %d", value);
		if (value) {
			DCC_LOG(LOG_TRACE, "[CONFIGURED]");
			dev[0].in_ep = usb_dev_ep_init(usb, &usb_mon_in_info, NULL, 0);
			dev[0].out_ep = usb_dev_ep_init(usb, &usb_mon_out_info, NULL, 0);
			dev[0].int_ep = usb_dev_ep_init(usb, &usb_mon_int_info, NULL, 0);
			usb_dev_ep_ctl(usb, dev->out_ep, USB_EP_RECV_OK);
			dev->configured = 1;
		} else {
			DCC_LOG(LOG_TRACE, "[UNCONFIGURED]");
			usb_dev_ep_ctl(dev->usb, dev[0].in_ep, USB_EP_DISABLE);
			usb_dev_ep_ctl(dev->usb, dev[0].out_ep, USB_EP_DISABLE);
			usb_dev_ep_ctl(dev->usb, dev[0].int_ep, USB_EP_DISABLE);
			dev->configured = 0;
		}
		break;
	}


	case STD_GET_STATUS_DEVICE:
		DCC_LOG(LOG_TRACE, "GetStatusDev");
		*ptr = (void *)&device_status;
		len = 2;
		break;

	case STD_GET_CONFIGURATION:
		DCC_LOG(LOG_TRACE, "GetCfg");
		*ptr = (void *)&dev->configured;
		len = 1;
		break;

#if 0
	case STD_CLEAR_FEATURE_DEVICE:
		if (value == USB_DEVICE_REMOTE_WAKEUP) {
		} else if (value == USB_TEST_MODE ) {
		}
		DCC_LOG(LOG_TRACE, "ClrFeatureDev(%d)", value);
		break;

	case STD_SET_FEATURE_DEVICE:
		if (value == USB_DEVICE_REMOTE_WAKEUP) {
		} else if (value == USB_TEST_MODE ) {
		}
		DCC_LOG(LOG_TRACE, "SetFeatureDev", value);
		break;
#endif

	/* Standard Interface Requests */
	case STD_GET_STATUS_INTERFACE:
		DCC_LOG1(LOG_TRACE, "GetStatusIf(%d)", index);
		*ptr = (void *)&interface_status;
		len = 2;
		break;

#if 0
	case STD_CLEAR_FEATURE_INTERFACE:
		DCC_LOG(LOG_TRACE, "ClrFeatureIf(%d,%d)", index, value);
		break;

	case STD_SET_FEATURE_INTERFACE:
		DCC_LOG(LOG_TRACE, "SetFeatureIf(%d,%d)", index, value);
		break;

	case STD_GET_INTERFACE:
		DCC_LOG(LOG_TRACE, "GetInterface(%d)", index);
		break;

	case STD_SET_INTERFACE:
		DCC_LOG(LOG_TRACE, "SetInterface(%d)", index);
		break;
#endif


	/* Standard Endpoint Requests */
#if 0
	case STD_GET_STATUS_ENDPOINT:
		index &= 0x0f;
		DCC_LOG1(LOG_TRACE, "GetStatusEpt:%d", index);
		break;

	case STD_CLEAR_FEATURE_ENDPOINT:
		index &= 0x0f;
		DCC_LOG1(LOG_TRACE, "ClrFeatureEP:%d", index);
		break;

	case STD_CLEAR_FEATURE_ENDPOINT:
		index &= 0x0f;
		DCC_LOG1(LOG_TRACE, "SetFeatureEP:%d", index);
		break;

	case STD_SYNCH_FRAME:
		index &= 0x0f;
		DCC_LOG1(LOG_TRACE, "SetFeatureEP:%d", index);
		break;
#endif


	/* -------------------------------------------------------------------- 
	 * Class specific requests 
	 */

	case SET_LINE_CODING: 
		{
			struct cdc_line_coding * lc;
			lc = (struct cdc_line_coding *)dev->ctl_buf;
			(void)lc;
			DCC_LOG(LOG_TRACE, "CDC SetLn");
			DCC_LOG1(LOG_TRACE, "dsDTERate=%d", lc->dwDTERate);
			DCC_LOG1(LOG_TRACE, "bCharFormat=%d", lc->bCharFormat);
			DCC_LOG1(LOG_TRACE, "bParityType=%d", lc->bParityType);
			DCC_LOG1(LOG_TRACE, "bDataBits=%d", lc->bDataBits);
			break;
		}

	case GET_LINE_CODING:
		DCC_LOG(LOG_TRACE, "CDC GetLn");
		/* Return Line Coding */
		*ptr = (void *)&usb_cdc_lc;
		len = sizeof(struct cdc_line_coding);
		break;

	case SET_CONTROL_LINE_STATE:
		dev->acm_ctrl = value;
		DCC_LOG2(LOG_TRACE, "CDC_DTE_PRESENT=%d ACTIVATE_CARRIER=%d",
				(value & CDC_DTE_PRESENT) ? 1 : 0,
				(value & CDC_ACTIVATE_CARRIER) ? 1 : 0);
		/* signal monitor */
		dbgmon_signal(DBGMON_COMM_CTL);
		break;

	default:
		DCC_LOG5(LOG_WARNING, "CDC t=%x r=%x v=%x i=%d l=%d",
				req->type, req->request, value, req->index, len);
		break;
	}

	return len;
}

static const usb_dev_ep_info_t usb_mon_ep0_info = {
	.addr = 0,
	.attr = ENDPOINT_TYPE_CONTROL,
	.mxpktsz = EP0_MAX_PKT_SIZE,
	.on_setup = usb_mon_on_setup
};

static void usb_mon_on_reset(usb_class_t * cl)
{
	struct usb_cdc_acm_dev * dev = (struct usb_cdc_acm_dev *)cl;
	DCC_LOG(LOG_MSG, "...");
	dev->tx_seq = 0;
	dev->tx_ack = 0;
	dev->rx_seq = 0;
	dev->rx_ack = 0;
	/* clear input buffer */
	dev->rx_cnt = 0;
	dev->rx_pos = 0;
	/* reset control lines */
	dev->acm_ctrl = 0;
	/* initializes EP0 */
	dev->ctl_ep = usb_dev_ep_init(dev->usb, &usb_mon_ep0_info, 
								  dev->ctl_buf, CDC_CTL_BUF_LEN);
}

static void usb_mon_on_suspend(usb_class_t * cl)
{
	struct usb_cdc_acm_dev * dev = (struct usb_cdc_acm_dev *)cl;
	DCC_LOG(LOG_TRACE, "...");
	dev->acm_ctrl = 0;
}

static void usb_mon_on_wakeup(usb_class_t * cl)
{
	DCC_LOG(LOG_TRACE, "...");
}

static void usb_mon_on_error(usb_class_t * cl, int code)
{
}

static int usb_comm_send(const void * comm, const void * buf, unsigned int len)
{
	struct usb_cdc_acm_dev * dev = (struct usb_cdc_acm_dev *)comm;
	uint8_t * ptr = (uint8_t *)buf;
	unsigned int rem;
	uint32_t seq;
	int ret;
	int n;

	if (dev->acm_ctrl == 0) {
//		DCC_LOG(LOG_MSG, "not connected!");
		dev->tx_seq = 0;
		dev->tx_ack = 0;
		dev->rx_seq = 0;
		dev->rx_ack = 0;
		/* not connected, discard!! */
		return len;
	}

	rem = len;
	seq = dev->tx_seq;
	while (rem) {
		n = usb_dev_ep_pkt_xmit(dev->usb, dev->in_ep, ptr, rem);
		DCC_LOG2(LOG_TRACE, "usb_dev_ep_pkt_xmit(%d) %d", rem, n);
		if (n < 0) {
#if THINKOS_DBGMON_ENABLE_COMM_STATS
			DCC_LOG1(LOG_WARNING, "usb_dev_ep_pkt_xmit() failed (pkt=%d)!", 
					 dev->stats.tx_pkt);
#else
			DCC_LOG(LOG_WARNING, "usb_dev_ep_pkt_xmit() failed");
#endif
			dev->tx_seq = seq;
			return n;
		} else if (n > 0) {
#if THINKOS_DBGMON_ENABLE_COMM_STATS
			dev->stats.tx_pkt++;
#endif
			rem -= n;
			ptr += n;
			seq += n;
		}  else {
			dev->tx_seq = seq;
			DCC_LOG(LOG_MSG, "dbgmon_expect(DBGMON_COMM_EOT)!");
			if ((ret = dbgmon_expect(DBGMON_COMM_EOT)) < 0) {
				DCC_LOG(LOG_WARNING, "dbgmon_expect()!");
				return ret;
			}
		}
	}


	dev->tx_seq = seq;
	DCC_LOG1(LOG_TRACE, "return=%d.", len - rem);

	return len - rem;
}

//int dmon_comm_recv(struct dmon_comm * comm, void * buf, unsigned int len)
static int usb_comm_recv(const void * comm, void * buf, unsigned int len)
{
	struct usb_cdc_acm_dev * dev = (struct usb_cdc_acm_dev *)comm;
	uint8_t * dst = (uint8_t *)buf;
	uint32_t ack;
	unsigned int pos;
	unsigned int cnt;
	unsigned int n;
	int ret;

	ack = dev->rx_ack;
//	while ((n = (int32_t)(dev->rx_seq - ack)) == 0) {
	do {
		DCC_LOG2(LOG_TRACE, "ack=%d n=%d blocked!!!", ack, n);
		if ((ret = dbgmon_expect(DBGMON_COMM_RCV)) < 0) {
			DCC_LOG(LOG_WARNING, "dbgmon_expect()!");
			return ret;
		}
	 } while ((n = (int32_t)(dev->rx_seq - ack)) == 0);

	if (dbgmon_is_set(DBGMON_COMM_RCV)) {
		DCC_LOG(LOG_WARNING, "dbgmon_is_set()!");
	}

	cnt = MIN(n, len);
	pos = ack % CDC_EP_IN_MAX_PKT_SIZE;

	if (pos == 0) {
		DCC_LOG4(LOG_TRACE, "1. n=%d ack=%d pos=%d cnt=%d...", 
				 n, ack, pos, cnt);
		__thinkos_memcpy(dst, dev->rx_buf, cnt);
	} else {
		unsigned int m = CDC_EP_IN_MAX_PKT_SIZE - pos;
		unsigned int l;

		m = MIN(m, cnt);
		__thinkos_memcpy(dst, &dev->rx_buf[pos], m);
		dst += m;

		l = cnt - m;
		__thinkos_memcpy(dst, dev->rx_buf, l);

		DCC_LOG6(LOG_TRACE, "2. m=%d l=%d n=%d ack=%d pos=%d cnt=%d...", 
				 m, l, n, ack, pos, cnt);
	}

	ack += cnt;
	if ((int32_t)(dev->rx_seq - ack) > 0) {
		DCC_LOG(LOG_WARNING, "signal DBGMON_COMM_RCV!");
		/* Pending data on fifo, resignal .. */
		dbgmon_signal(DBGMON_COMM_RCV);
	}

	dev->rx_ack = ack;

	return cnt;
}

static int usb_comm_connect(const void * comm)
{
	struct usb_cdc_acm_dev * dev = (struct usb_cdc_acm_dev *)comm;

	int ret;

	while ((dev->acm_ctrl & CDC_DTE_PRESENT) == 0) {
		DCC_LOG1(LOG_TRACE, "ctrl=%02x, waiting...", dev->acm_ctrl);
		if ((ret = dbgmon_expect(DBGMON_COMM_CTL)) < 0) {
			DCC_LOG1(LOG_WARNING, "ret=%d!!", ret);
			return ret;
		}
	}

	return 0;
}

static bool usb_comm_isconnected(const void * comm)
{
	struct usb_cdc_acm_dev * dev = (struct usb_cdc_acm_dev *)comm;

	/* Pending data on fifo, resignal .. */
	if ((int32_t)(dev->rx_seq - dev->rx_ack) > 0) {
		DCC_LOG(LOG_WARNING, "signal DBGMON_COMM_RCV!");
		dbgmon_signal(DBGMON_COMM_RCV);
	}

	return (dev->acm_ctrl & CDC_DTE_PRESENT) ? true : false;
}

static struct usb_class_if usb_class_if_instance;

static const usb_class_events_t usb_mon_ev = {
	.on_reset = usb_mon_on_reset,
	.on_suspend = usb_mon_on_suspend,
	.on_wakeup = usb_mon_on_wakeup,
	.on_error = usb_mon_on_error
};

static const struct dbgmon_comm_op usb_cdc_comm_op = {
	.send = usb_comm_send,
	.recv = usb_comm_recv,
	.connect = usb_comm_connect,
	.isconnected = usb_comm_isconnected
};

static const struct dbgmon_comm usb_comm_instance = {
	.dev = (void *)&usb_class_if_instance,
	.op = &usb_cdc_comm_op,
};

const struct dbgmon_comm * usb_comm_queue_init(const usb_dev_t * usb)
{
	struct usb_cdc_acm_dev * dev = &usb_class_if_instance.dev;
	struct usb_class_if * cl = &usb_class_if_instance;

	/* initialize USB device */
	dev->usb = (usb_dev_t *)usb;
	dev->rx_cnt = 0;
	dev->rx_pos = 0;
	dev->tx_seq = 0; 
	dev->tx_ack = 0; 
	dev->rx_seq = 0;
	dev->rx_ack = 0;
	dev->rx_paused = false;
	dev->configured = 0;

	DCC_LOG(LOG_TRACE, "usb_dev_init()");
	usb_dev_init(dev->usb, cl, &usb_mon_ev);

	return &usb_comm_instance;
}

#endif

