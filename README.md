# m12-dds2can-subscribers
This is the project for subscribing M12 command information and writing them to CAN bus.

# Dependencies
This program uses [RTI Connext](https://www.rti.com/products) which needs an active license. You can get [a 30-day free trial license from RTI](https://www.rti.com/free-trial).

# What does it do
This is the interface for commanding the m12 wheel loader. After initialization, it blocks execution and listens for DDS messages. When a publication is received, the information from the DDS message is converted to CAN bus form and written to the CAN bus. 
