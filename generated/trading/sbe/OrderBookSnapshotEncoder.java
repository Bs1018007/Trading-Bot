/* Generated SBE (Simple Binary Encoding) message codec. */
package trading.sbe;

import org.agrona.MutableDirectBuffer;
import org.agrona.DirectBuffer;


/**
 * Orderbook snapshot
 */
@SuppressWarnings("all")
public final class OrderBookSnapshotEncoder
{
    public static final int BLOCK_LENGTH = 12;
    public static final int TEMPLATE_ID = 2;
    public static final int SCHEMA_ID = 1;
    public static final int SCHEMA_VERSION = 0;
    public static final String SEMANTIC_VERSION = "0.1";
    public static final java.nio.ByteOrder BYTE_ORDER = java.nio.ByteOrder.LITTLE_ENDIAN;

    private final OrderBookSnapshotEncoder parentMessage = this;
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

    public OrderBookSnapshotEncoder wrap(final MutableDirectBuffer buffer, final int offset)
    {
        if (buffer != this.buffer)
        {
            this.buffer = buffer;
        }
        this.offset = offset;
        limit(offset + BLOCK_LENGTH);

        return this;
    }

    public OrderBookSnapshotEncoder wrapAndApplyHeader(
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

    public OrderBookSnapshotEncoder timestamp(final long value)
    {
        buffer.putLong(offset + 0, value, BYTE_ORDER);
        return this;
    }


    public static int bidCountId()
    {
        return 2;
    }

    public static int bidCountSinceVersion()
    {
        return 0;
    }

    public static int bidCountEncodingOffset()
    {
        return 8;
    }

    public static int bidCountEncodingLength()
    {
        return 2;
    }

    public static String bidCountMetaAttribute(final MetaAttribute metaAttribute)
    {
        if (MetaAttribute.PRESENCE == metaAttribute)
        {
            return "required";
        }

        return "";
    }

    public static int bidCountNullValue()
    {
        return 65535;
    }

    public static int bidCountMinValue()
    {
        return 0;
    }

    public static int bidCountMaxValue()
    {
        return 65534;
    }

    public OrderBookSnapshotEncoder bidCount(final int value)
    {
        buffer.putShort(offset + 8, (short)value, BYTE_ORDER);
        return this;
    }


    public static int askCountId()
    {
        return 3;
    }

    public static int askCountSinceVersion()
    {
        return 0;
    }

    public static int askCountEncodingOffset()
    {
        return 10;
    }

    public static int askCountEncodingLength()
    {
        return 2;
    }

    public static String askCountMetaAttribute(final MetaAttribute metaAttribute)
    {
        if (MetaAttribute.PRESENCE == metaAttribute)
        {
            return "required";
        }

        return "";
    }

    public static int askCountNullValue()
    {
        return 65535;
    }

    public static int askCountMinValue()
    {
        return 0;
    }

    public static int askCountMaxValue()
    {
        return 65534;
    }

    public OrderBookSnapshotEncoder askCount(final int value)
    {
        buffer.putShort(offset + 10, (short)value, BYTE_ORDER);
        return this;
    }


    private final BidsEncoder bids = new BidsEncoder(this);

    public static long bidsId()
    {
        return 4;
    }

    public BidsEncoder bidsCount(final int count)
    {
        bids.wrap(buffer, count);
        return bids;
    }

    public static final class BidsEncoder
    {
        public static final int HEADER_SIZE = 4;
        private final OrderBookSnapshotEncoder parentMessage;
        private MutableDirectBuffer buffer;
        private int count;
        private int index;
        private int offset;
        private int initialLimit;

        BidsEncoder(final OrderBookSnapshotEncoder parentMessage)
        {
            this.parentMessage = parentMessage;
        }

        public void wrap(final MutableDirectBuffer buffer, final int count)
        {
            if (count < 0 || count > 65534)
            {
                throw new IllegalArgumentException("count outside allowed range: count=" + count);
            }

            if (buffer != this.buffer)
            {
                this.buffer = buffer;
            }

            index = 0;
            this.count = count;
            final int limit = parentMessage.limit();
            initialLimit = limit;
            parentMessage.limit(limit + HEADER_SIZE);
            buffer.putShort(limit + 0, (short)16, BYTE_ORDER);
            buffer.putShort(limit + 2, (short)count, BYTE_ORDER);
        }

        public BidsEncoder next()
        {
            if (index >= count)
            {
                throw new java.util.NoSuchElementException();
            }

            offset = parentMessage.limit();
            parentMessage.limit(offset + sbeBlockLength());
            ++index;

            return this;
        }

        public int resetCountToIndex()
        {
            count = index;
            buffer.putShort(initialLimit + 2, (short)count, BYTE_ORDER);

            return count;
        }

        public static int countMinValue()
        {
            return 0;
        }

        public static int countMaxValue()
        {
            return 65534;
        }

        public static int sbeHeaderSize()
        {
            return HEADER_SIZE;
        }

        public static int sbeBlockLength()
        {
            return 16;
        }

        public static int priceId()
        {
            return 5;
        }

        public static int priceSinceVersion()
        {
            return 0;
        }

        public static int priceEncodingOffset()
        {
            return 0;
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

        public BidsEncoder price(final double value)
        {
            buffer.putDouble(offset + 0, value, BYTE_ORDER);
            return this;
        }


        public static int quantityId()
        {
            return 6;
        }

        public static int quantitySinceVersion()
        {
            return 0;
        }

        public static int quantityEncodingOffset()
        {
            return 8;
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

        public BidsEncoder quantity(final double value)
        {
            buffer.putDouble(offset + 8, value, BYTE_ORDER);
            return this;
        }

    }

    private final AsksEncoder asks = new AsksEncoder(this);

    public static long asksId()
    {
        return 7;
    }

    public AsksEncoder asksCount(final int count)
    {
        asks.wrap(buffer, count);
        return asks;
    }

    public static final class AsksEncoder
    {
        public static final int HEADER_SIZE = 4;
        private final OrderBookSnapshotEncoder parentMessage;
        private MutableDirectBuffer buffer;
        private int count;
        private int index;
        private int offset;
        private int initialLimit;

        AsksEncoder(final OrderBookSnapshotEncoder parentMessage)
        {
            this.parentMessage = parentMessage;
        }

        public void wrap(final MutableDirectBuffer buffer, final int count)
        {
            if (count < 0 || count > 65534)
            {
                throw new IllegalArgumentException("count outside allowed range: count=" + count);
            }

            if (buffer != this.buffer)
            {
                this.buffer = buffer;
            }

            index = 0;
            this.count = count;
            final int limit = parentMessage.limit();
            initialLimit = limit;
            parentMessage.limit(limit + HEADER_SIZE);
            buffer.putShort(limit + 0, (short)16, BYTE_ORDER);
            buffer.putShort(limit + 2, (short)count, BYTE_ORDER);
        }

        public AsksEncoder next()
        {
            if (index >= count)
            {
                throw new java.util.NoSuchElementException();
            }

            offset = parentMessage.limit();
            parentMessage.limit(offset + sbeBlockLength());
            ++index;

            return this;
        }

        public int resetCountToIndex()
        {
            count = index;
            buffer.putShort(initialLimit + 2, (short)count, BYTE_ORDER);

            return count;
        }

        public static int countMinValue()
        {
            return 0;
        }

        public static int countMaxValue()
        {
            return 65534;
        }

        public static int sbeHeaderSize()
        {
            return HEADER_SIZE;
        }

        public static int sbeBlockLength()
        {
            return 16;
        }

        public static int priceId()
        {
            return 8;
        }

        public static int priceSinceVersion()
        {
            return 0;
        }

        public static int priceEncodingOffset()
        {
            return 0;
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

        public AsksEncoder price(final double value)
        {
            buffer.putDouble(offset + 0, value, BYTE_ORDER);
            return this;
        }


        public static int quantityId()
        {
            return 9;
        }

        public static int quantitySinceVersion()
        {
            return 0;
        }

        public static int quantityEncodingOffset()
        {
            return 8;
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

        public AsksEncoder quantity(final double value)
        {
            buffer.putDouble(offset + 8, value, BYTE_ORDER);
            return this;
        }

    }

    public static int symbolId()
    {
        return 10;
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

    public OrderBookSnapshotEncoder putSymbol(final DirectBuffer src, final int srcOffset, final int length)
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

    public OrderBookSnapshotEncoder putSymbol(final byte[] src, final int srcOffset, final int length)
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

        final OrderBookSnapshotDecoder decoder = new OrderBookSnapshotDecoder();
        decoder.wrap(buffer, offset, BLOCK_LENGTH, SCHEMA_VERSION);

        return decoder.appendTo(builder);
    }
}
