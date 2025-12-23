/* Generated SBE (Simple Binary Encoding) message codec. */
package trading.sbe;

import org.agrona.MutableDirectBuffer;
import org.agrona.DirectBuffer;


/**
 * Trading signal
 */
@SuppressWarnings("all")
public final class TradeSignalEncoder
{
    public static final int BLOCK_LENGTH = 25;
    public static final int TEMPLATE_ID = 3;
    public static final int SCHEMA_ID = 1;
    public static final int SCHEMA_VERSION = 0;
    public static final String SEMANTIC_VERSION = "0.1";
    public static final java.nio.ByteOrder BYTE_ORDER = java.nio.ByteOrder.LITTLE_ENDIAN;

    private final TradeSignalEncoder parentMessage = this;
    private MutableDirectBuffer buffer;
    private int offset;
    private int limit;

    public int sbeBlockLength()
    {
        return BLOCK_LENGTH;
    }

    public int sbeTemplateId()
    {
        return TEMPLATE_ID;
    }

    public int sbeSchemaId()
    {
        return SCHEMA_ID;
    }

    public int sbeSchemaVersion()
    {
        return SCHEMA_VERSION;
    }

    public String sbeSemanticType()
    {
        return "";
    }

    public MutableDirectBuffer buffer()
    {
        return buffer;
    }

    public int offset()
    {
        return offset;
    }

    public TradeSignalEncoder wrap(final MutableDirectBuffer buffer, final int offset)
    {
        if (buffer != this.buffer)
        {
            this.buffer = buffer;
        }
        this.offset = offset;
        limit(offset + BLOCK_LENGTH);

        return this;
    }

    public TradeSignalEncoder wrapAndApplyHeader(
        final MutableDirectBuffer buffer, final int offset, final MessageHeaderEncoder headerEncoder)
    {
        headerEncoder
            .wrap(buffer, offset)
            .blockLength(BLOCK_LENGTH)
            .templateId(TEMPLATE_ID)
            .schemaId(SCHEMA_ID)
            .version(SCHEMA_VERSION);

        return wrap(buffer, offset + MessageHeaderEncoder.ENCODED_LENGTH);
    }

    public int encodedLength()
    {
        return limit - offset;
    }

    public int limit()
    {
        return limit;
    }

    public void limit(final int limit)
    {
        this.limit = limit;
    }

    public static int timestampId()
    {
        return 1;
    }

    public static int timestampSinceVersion()
    {
        return 0;
    }

    public static int timestampEncodingOffset()
    {
        return 0;
    }

    public static int timestampEncodingLength()
    {
        return 8;
    }

    public static String timestampMetaAttribute(final MetaAttribute metaAttribute)
    {
        if (MetaAttribute.PRESENCE == metaAttribute)
        {
            return "required";
        }

        return "";
    }

    public static long timestampNullValue()
    {
        return 0xffffffffffffffffL;
    }

    public static long timestampMinValue()
    {
        return 0x0L;
    }

    public static long timestampMaxValue()
    {
        return 0xfffffffffffffffeL;
    }

    public TradeSignalEncoder timestamp(final long value)
    {
        buffer.putLong(offset + 0, value, BYTE_ORDER);
        return this;
    }


    public static int actionId()
    {
        return 2;
    }

    public static int actionSinceVersion()
    {
        return 0;
    }

    public static int actionEncodingOffset()
    {
        return 8;
    }

    public static int actionEncodingLength()
    {
        return 1;
    }

    public static String actionMetaAttribute(final MetaAttribute metaAttribute)
    {
        if (MetaAttribute.PRESENCE == metaAttribute)
        {
            return "required";
        }

        return "";
    }

    public static short actionNullValue()
    {
        return (short)255;
    }

    public static short actionMinValue()
    {
        return (short)0;
    }

    public static short actionMaxValue()
    {
        return (short)254;
    }

    public TradeSignalEncoder action(final short value)
    {
        buffer.putByte(offset + 8, (byte)value);
        return this;
    }


    public static int priceId()
    {
        return 3;
    }

    public static int priceSinceVersion()
    {
        return 0;
    }

    public static int priceEncodingOffset()
    {
        return 9;
    }

    public static int priceEncodingLength()
    {
        return 8;
    }

    public static String priceMetaAttribute(final MetaAttribute metaAttribute)
    {
        if (MetaAttribute.PRESENCE == metaAttribute)
        {
            return "required";
        }

        return "";
    }

    public static double priceNullValue()
    {
        return Double.NaN;
    }

    public static double priceMinValue()
    {
        return -1.7976931348623157E308d;
    }

    public static double priceMaxValue()
    {
        return 1.7976931348623157E308d;
    }

    public TradeSignalEncoder price(final double value)
    {
        buffer.putDouble(offset + 9, value, BYTE_ORDER);
        return this;
    }


    public static int quantityId()
    {
        return 4;
    }

    public static int quantitySinceVersion()
    {
        return 0;
    }

    public static int quantityEncodingOffset()
    {
        return 17;
    }

    public static int quantityEncodingLength()
    {
        return 8;
    }

    public static String quantityMetaAttribute(final MetaAttribute metaAttribute)
    {
        if (MetaAttribute.PRESENCE == metaAttribute)
        {
            return "required";
        }

        return "";
    }

    public static double quantityNullValue()
    {
        return Double.NaN;
    }

    public static double quantityMinValue()
    {
        return -1.7976931348623157E308d;
    }

    public static double quantityMaxValue()
    {
        return 1.7976931348623157E308d;
    }

    public TradeSignalEncoder quantity(final double value)
    {
        buffer.putDouble(offset + 17, value, BYTE_ORDER);
        return this;
    }


    public static int symbolId()
    {
        return 5;
    }

    public static String symbolMetaAttribute(final MetaAttribute metaAttribute)
    {
        if (MetaAttribute.PRESENCE == metaAttribute)
        {
            return "required";
        }

        return "";
    }

    public static int symbolHeaderLength()
    {
        return 2;
    }

    public TradeSignalEncoder putSymbol(final DirectBuffer src, final int srcOffset, final int length)
    {
        if (length > 65534)
        {
            throw new IllegalStateException("length > maxValue for type: " + length);
        }

        final int headerLength = 2;
        final int limit = parentMessage.limit();
        parentMessage.limit(limit + headerLength + length);
        buffer.putShort(limit, (short)length, BYTE_ORDER);
        buffer.putBytes(limit + headerLength, src, srcOffset, length);

        return this;
    }

    public TradeSignalEncoder putSymbol(final byte[] src, final int srcOffset, final int length)
    {
        if (length > 65534)
        {
            throw new IllegalStateException("length > maxValue for type: " + length);
        }

        final int headerLength = 2;
        final int limit = parentMessage.limit();
        parentMessage.limit(limit + headerLength + length);
        buffer.putShort(limit, (short)length, BYTE_ORDER);
        buffer.putBytes(limit + headerLength, src, srcOffset, length);

        return this;
    }

    public String toString()
    {
        if (null == buffer)
        {
            return "";
        }

        return appendTo(new StringBuilder()).toString();
    }

    public StringBuilder appendTo(final StringBuilder builder)
    {
        if (null == buffer)
        {
            return builder;
        }

        final TradeSignalDecoder decoder = new TradeSignalDecoder();
        decoder.wrap(buffer, offset, BLOCK_LENGTH, SCHEMA_VERSION);

        return decoder.appendTo(builder);
    }
}
