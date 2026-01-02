# Flash Memory Lifetime Analysis

## Usage Scenario
- **Per event**: <100 link updates (taps)
- **Events per year**: Multiple (assumed 10 events/year for calculations)
- **Storage size**: ~896 bytes per write

## Internal Flash (STM32L0) Analysis

### Write Cycle Limit
- **Endurance**: ~10,000 erase/write cycles per page
- **Current implementation**: 2-second delayed write batching
- **Write strategy**: Each full write consumes one cycle regardless of data changes

### Lifetime Calculations

#### Scenario 1: Worst Case (No Batching)
- All 100 taps spread >2 seconds apart
- **Writes per event**: 100
- **Writes per year** (10 events): 1,000
- **Estimated lifetime**: **10 years**
- ⚠️ **Risk**: High risk of premature failure

#### Scenario 2: Realistic Case (Partial Batching)
- Taps come in bursts during events
- Some batching occurs (multiple taps within 2s windows)
- Average 2-3 taps batched together
- **Writes per event**: ~40-50
- **Writes per year** (10 events): 400-500
- **Estimated lifetime**: **20-25 years**
- ✅ **Assessment**: Acceptable for most use cases

#### Scenario 3: Best Case (Maximum Batching)
- All 100 taps happen within 2-second windows
- **Writes per event**: 1-2
- **Writes per year** (10 events): 10-20
- **Estimated lifetime**: **500-1000 years**
- ℹ️ **Note**: Unrealistic for live events

### Considerations

1. **Temperature Effects**: Higher operating temperatures reduce flash endurance
2. **Safety Margin**: Real-world failures may occur before reaching theoretical limits
3. **Variability**: Individual flash pages may have different characteristics
4. **Additional Writes**: Provisioning, clears, and other operations consume cycles

## External Flash Recommendation

### Why External Flash?

For **production devices** with long-term reliability requirements, **external SPI flash** is recommended:

#### Advantages
1. **Higher Endurance**: Typically 100,000+ erase/write cycles (10x better)
2. **Better Reliability**: Designed for frequent writes
3. **Larger Capacity**: More space for logs, data history
4. **Encryption**: Can encrypt using provisioned device key for security
5. **Isolation**: Wear doesn't affect main MCU flash (firmware storage)

#### Implementation Considerations
- **Encryption**: Use AES-256 with provisioned device key
- **Interface**: SPI or QSPI for fast access
- **Library**: Use existing STM32 SPI flash libraries (e.g., LittleFS, SPIFFS)
- **Wear Leveling**: Many flash chips include built-in wear leveling
- **Cost**: Adds ~$0.10-0.50 per unit (depending on capacity)

#### Recommended Chips
- **Winbond W25Q**: 64KB-128KB, 100,000 cycles
- **Macronix MX25**: Good STM32 compatibility
- **Adesto AT25**: Low power options

### Estimated Lifetime with External Flash

- **Endurance**: 100,000+ cycles
- **Writes per event** (realistic): 40-50
- **Writes per year** (10 events): 400-500
- **Estimated lifetime**: **200+ years**
- ✅ **Assessment**: Excellent for production use

## Recommendations

### For Prototyping/Evaluation
✅ **Internal flash is acceptable** with current 2s batching:
- Realistic estimate: ~20-25 years at 10 events/year
- Sufficient for testing and early deployments
- Monitor actual write patterns in field

### For Production
✅ **Use external flash** if:
- Long-term reliability is critical (>10 years)
- High-frequency usage expected
- Device should outlast warranty period significantly
- Additional storage capacity needed

### Hybrid Approach
Consider using:
- **Internal flash**: For critical data (device ID, provisioned key)
- **External flash**: For frequently changing data (links, tap counts)

This provides:
- Critical data safety (stored in MCU)
- High endurance for dynamic data (external flash)
- Encryption on external flash using provisioned key

## Implementation Notes

If switching to external flash:

1. **Encryption Layer**: Encrypt all data using AES-256 with device provisioned key
2. **Storage Layer**: Use existing filesystem (LittleFS recommended for STM32)
3. **Migration**: Consider backward compatibility for existing devices
4. **Testing**: Thoroughly test write endurance in your usage pattern

## Conclusion

For your usage pattern (<100 links/event, multiple events/year):
- **Internal flash**: ~20-25 years (realistic with 2s batching) - acceptable for many use cases
- **External flash**: 200+ years - recommended for production reliability

The choice depends on your reliability requirements, expected device lifetime, and cost constraints.

