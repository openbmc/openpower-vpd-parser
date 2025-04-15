#pragma once

namespace vpd
{
/**
 * @brief Class to handle specific use case.
 *
 * Few pre-requisites needs to be taken case specifically, which will be
 * encapsulated by this class.
 */
class IbmHandler
{
    public:
    /**
     * List of deleted methods.
     */
    IbmHandler(const IbmHandler&) = delete;
    IbmHandler& operator=(const IbmHandler&) = delete;
    IbmHandler(IbmHandler&&) = delete;

    /**
     * @brief Constructor.
     */
    IbmHandler(){}
};
}//namespace vpd
