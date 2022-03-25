void TCA9548A(uint8_t bus)
{
  Wire1.beginTransmission(0x70);  // TCA9548A address is 0x70
  Wire1.write(1 << bus);          // send byte to select bus
  Wire1.endTransmission();
}

