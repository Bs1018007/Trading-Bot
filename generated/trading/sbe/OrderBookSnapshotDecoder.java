/* Generated SBE (Simple Binary Encoding) message codec. */
package trading.sbe;

import org.agrona.MutableDirectBuffer;
import org.agrona.DirectBuffer;


/**
 * Orderbook snapshot
 */
@SuppressWarnings("all")
public final class OrderBookSnapshotDecoder
{
    public static final int BLOCK_LENGTH = 12;
    public static final int TEMPLATE_ID = 2;
    public static final int SCHEMA_ID = 1;
    public static final int SCHEMA_VERSION = 0;
    public static final String SEMANTIC_VERSION = "0.1";
    public static final java.nio.ByteOrder BYTE_ORDER = java.nio.ByteOrder.LITTLE_ENDIAN;

    private final OrderBookSnapshotDecoder parentMessage = this;
    private DirectBuffer buffer;
    private int offset;
    private int limit;
    int actingBlockLength;
    int actingVersion;

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

    public DirectBuffer buffer()
    {
        return buffer;
    }

    public int offset()
    {
        return offset;
    }

    public OrderBookSnapshotDecoder wrap(
        final DirectBuffer buffer,
        final int offset,
        final int actingBlockLength,
        final int actingVersion)
    {
        if (buffer != this.buffer)
        {
            this.buffer = buffer;
        }
        this.offset = offset;
        this.actingBlockLength = actingBlockLength;
        this.actingVersion = actingVersion;
        limit(offset + actingBlockLength);

        return this;
    }

    public OrderBookSnapshotDecoder wrapAndApplyHeader(
        final DirectBuffer buffer,
        final int offset,
        final MessageHeaderDecoder headerDecoder)
    {
        headerDecoder.wrap(buffer, offset);

        final int templateId = headerDecoder.templateId();
        if (TEMPLATE_ID != templateId)
        {
            throw new IllegalStateException("Invalid TEMPLATE_ID: " + templateId);
        }

        return wrap(
            buffer,
            offset + MessageHeaderDecoder.ENCODED_LENGTH,
            headerDecoder.blockLength(),
            headerDecoder.version());
    }

    public OrderBookSnapshotDecoder sbeRewind()
    {
        return wrap(buffer, offset, actingBlockLength, actingVersion);
    }

    public int sbeDecodedLength()
    {
        final int currentLimit = limit();
        sbeSkip();
        final int decodedLength = encodedLength();
        limit(currentLimit);

        return decodedLength;
    }

    public int actingVersion()
    {
        return actingVersion;
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

    public long timestamp()
    {
        return buffer.getLong(offset + 0, BYTE_ORDER);
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

    public int bidCount()
    {
        return (buffer.getShort(offset + 8, BYTE_ORDER) & 0xFFFF);
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

    public int askCount()
    {
        return (buffer.getShort(offset + 10, BYTE_ORDER) & 0xFFFF);
    }


    private final BidsDecoder bids = new BidsDecoder(this);

    public static long bidsDecoderId()
    {
        return 4;
    }

    public static int bidsDecoderSinceVersion()
    {
        return 0;
    }

    public BidsDecoder bids()
    {
        bids.wrap(buffer);
        return bids;
    }

    public static final class BidsDecoder
        implements Iterable<BidsDecoder>, java.util.Iterator<BidsDecoder>
    {
        public static final int HEADER_SIZE = 4;
        private final OrderBookSnapshotDecoder parentMessage;
        private DirectBuffer buffer;
        private int count;
        private int index;
        private int offset;
        private int blockLength;

        BidsDecoder(final OrderBookSnapshotDecoder parentMessage)
        {
            this.parentMessage = parentMessage;
        }

        public void wrap(final DirectBuffer buffer)
        {
            if (buffer != this.buffer)
            {
                this.buffer = buffer;
            }

            index = 0;
            final int limit = parentMessage.limit();
            parentMessage.limit(limit + HEADER_SIZE);
            blockLength = (buffer.getShort(limit + 0, BYTE_ORDER) & 0xFFFF);
            count = (buffer.getShort(limit + 2, BYTE_ORDER) & 0xFFFF);
        }

        public BidsDecoder next()
        {
            if (index >= count)
            {
                throw new java.util.NoSuchElementException();
            }

            offset = parentMessage.limit();
            parentMessage.limit(offset + blockLength);
            ++index;

            return this;
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

        public int actingBlockLength()
        {
            return blockLength;
        }

        public int actingVersion()
        {
            return parentMessage.actingVersion;
        }

        public int count()
        {
            return count;
        }

        public java.util.Iterator<BidsDecoder> iterator()
        {
            return this;
        }

        public void remove()
        {
            throw new UnsupportedOperationException();
        }

        public boolean hasNext()
        {
            return index < count;
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

        public double price()
        {
            return buffer.getDouble(offset + 0, BYTE_ORDER);
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

        public double quantity()
        {
            return buffer.getDouble(offset + 8, BYTE_ORDER);
        }


        public StringBuilder appendTo(final StringBuilder builder)
        {
            if (null == buffer)
            {
                return builder;
            }

            builder.append('(');
            builder.append("price=");
            builder.append(this.price());
            builder.append('|');
            builder.append("quantity=");
            builder.append(this.quantity());
            builder.append(')');

            return builder;
        }
        
        public BidsDecoder sbeSkip()
        {

            return this;
        }
    }

    private final AsksDecoder asks = new AsksDecoder(this);

    public static long asksDecoderId()
    {
        return 7;
    }

    public static int asksDecoderSinceVersion()
    {
        return 0;
    }

    public AsksDecoder asks()
    {
        asks.wrap(buffer);
        return asks;
    }

    public static final class AsksDecoder
        implements Iterable<AsksDecoder>, java.util.Iterator<AsksDecoder>
    {
        public static final int HEADER_SIZE = 4;
        private final OrderBookSnapshotDecoder parentMessage;
        private DirectBuffer buffer;
        private int count;
        private int index;
        private int offset;
        private int blockLength;

        AsksDecoder(final OrderBookSnapshotDecoder parentMessage)
        {
            this.parentMessage = parentMessage;
        }

        public void wrap(final DirectBuffer buffer)
        {
            if (buffer != this.buffer)
            {
                this.buffer = buffer;
            }

            index = 0;
            final int limit = parentMessage.limit();
            parentMessage.limit(limit + HEADER_SIZE);
            blockLength = (buffer.getShort(limit + 0, BYTE_ORDER) & 0xFFFF);
            count = (buffer.getShort(limit + 2, BYTE_ORDER) & 0xFFFF);
        }

        public AsksDecoder next()
        {
            if (index >= count)
            {
                throw new java.util.NoSuchElementException();
            }

            offset = parentMessage.limit();
            parentMessage.limit(offset + blockLength);
            ++index;

            return this;
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

        public int actingBlockLength()
        {
            return blockLength;
        }

        public int actingVersion()
        {
            return parentMessage.actingVersion;
        }

        public int count()
        {
            return count;
        }

        public java.util.Iterator<AsksDecoder> iterator()
        {
            return this;
        }

        public void remove()
        {
            throw new UnsupportedOperationException();
        }

        public boolean hasNext()
        {
            return index < count;
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

        public double price()
        {
            return buffer.getDouble(offset + 0, BYTE_ORDER);
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

        public double quantity()
        {
            return buffer.getDouble(offset + 8, BYTE_ORDER);
        }


        public StringBuilder appendTo(final StringBuilder builder)
        {
            if (null == buffer)
            {
                return builder;
            }

            builder.append('(');
            builder.append("price=");
            builder.append(this.price());
            builder.append('|');
            builder.append("quantity=");
            builder.append(this.quantity());
            builder.append(')');

            return builder;
        }
        
        public AsksDecoder sbeSkip()
        {

            return this;
        }
    }

    public static int symbolId()
    {
        return 10;
    }

    public static int symbolSinceVersion()
    {
        return 0;
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

    public int symbolLength()
    {
        final int limit = parentMessage.limit();
        return (buffer.getShort(limit, BYTE_ORDER) & 0xFFFF);
    }

    public int skipSymbol()
    {
        final int headerLength = 2;
        final int limit = parentMessage.limit();
        final int dataLength = (buffer.getShort(limit, BYTE_ORDER) & 0xFFFF);
        final int dataOffset = limit + headerLength;
        parentMessage.limit(dataOffset + dataLength);

        return dataLength;
    }

    public int getSymbol(final MutableDirectBuffer dst, final int dstOffset, final int length)
    {
        final int headerLength = 2;
        final int limit = parentMessage.limit();
        final int dataLength = (buffer.getShort(limit, BYTE_ORDER) & 0xFFFF);
        final int bytesCopied = Math.min(length, dataLength);
        parentMessage.limit(limit + headerLength + dataLength);
        buffer.getBytes(limit + headerLength, dst, dstOffset, bytesCopied);

        return bytesCopied;
    }

    public int getSymbol(final byte[] dst, final int dstOffset, final int length)
    {
        final int headerLength = 2;
        final int limit = parentMessage.limit();
        final int dataLength = (buffer.getShort(limit, BYTE_ORDER) & 0xFFFF);
        final int bytesCopied = Math.min(length, dataLength);
        parentMessage.limit(limit + headerLength + dataLength);
        buffer.getBytes(limit + headerLength, dst, dstOffset, bytesCopied);

        return bytesCopied;
    }

    public void wrapSymbol(final DirectBuffer wrapBuffer)
    {
        final int headerLength = 2;
        final int limit = parentMessage.limit();
        final int dataLength = (buffer.getShort(limit, BYTE_ORDER) & 0xFFFF);
        parentMessage.limit(limit + headerLength + dataLength);
        wrapBuffer.wrap(buffer, limit + headerLength, dataLength);
    }

    public String toString()
    {
        if (null == buffer)
        {
            return "";
        }

        final OrderBookSnapshotDecoder decoder = new OrderBookSnapshotDecoder();
        decoder.wrap(buffer, offset, actingBlockLength, actingVersion);

        return decoder.appendTo(new StringBuilder()).toString();
    }

    public StringBuilder appendTo(final StringBuilder builder)
    {
        if (null == buffer)
        {
            return builder;
        }

        final int originalLimit = limit();
        limit(offset + actingBlockLength);
        builder.append("[OrderBookSnapshot](sbeTemplateId=");
        builder.append(TEMPLATE_ID);
        builder.append("|sbeSchemaId=");
        builder.append(SCHEMA_ID);
        builder.append("|sbeSchemaVersion=");
        if (parentMessage.actingVersion != SCHEMA_VERSION)
        {
            builder.append(parentMessage.actingVersion);
            builder.append('/');
        }
        builder.append(SCHEMA_VERSION);
        builder.append("|sbeBlockLength=");
        if (actingBlockLength != BLOCK_LENGTH)
        {
            builder.append(actingBlockLength);
            builder.append('/');
        }
        builder.append(BLOCK_LENGTH);
        builder.append("):");
        builder.append("timestamp=");
        builder.append(this.timestamp());
        builder.append('|');
        builder.append("bidCount=");
        builder.append(this.bidCount());
        builder.append('|');
        builder.append("askCount=");
        builder.append(this.askCount());
        builder.append('|');
        builder.append("bids=[");
        final int bidsOriginalOffset = bids.offset;
        final int bidsOriginalIndex = bids.index;
        final BidsDecoder bids = this.bids();
        if (bids.count() > 0)
        {
            while (bids.hasNext())
            {
                bids.next().appendTo(builder);
                builder.append(',');
            }
            builder.setLength(builder.length() - 1);
        }
        bids.offset = bidsOriginalOffset;
        bids.index = bidsOriginalIndex;
        builder.append(']');
        builder.append('|');
        builder.append("asks=[");
        final int asksOriginalOffset = asks.offset;
        final int asksOriginalIndex = asks.index;
        final AsksDecoder asks = this.asks();
        if (asks.count() > 0)
        {
            while (asks.hasNext())
            {
                asks.next().appendTo(builder);
                builder.append(',');
            }
            builder.setLength(builder.length() - 1);
        }
        asks.offset = asksOriginalOffset;
        asks.index = asksOriginalIndex;
        builder.append(']');
        builder.append('|');
        builder.append("symbol=");
        builder.append(skipSymbol()).append(" bytes of raw data");

        limit(originalLimit);

        return builder;
    }
    
    public OrderBookSnapshotDecoder sbeSkip()
    {
        sbeRewind();
        BidsDecoder bids = this.bids();
        if (bids.count() > 0)
        {
            while (bids.hasNext())
            {
                bids.next();
                bids.sbeSkip();
            }
        }
        AsksDecoder asks = this.asks();
        if (asks.count() > 0)
        {
            while (asks.hasNext())
            {
                asks.next();
                asks.sbeSkip();
            }
        }
        skipSymbol();

        return this;
    }
}
