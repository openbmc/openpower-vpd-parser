namespace openpower
{
namespace vpd
{
template <typename T>
bool isPrintableData(const T& vpdData)
{
    bool printableChar = true;
    for (auto i : vpdData)
    {
        if (!isprint(i))
        {
            printableChar = false;
            break;
        }
    }
    return printableChar;
}
} // namespace vpd
} // namespace openpower
