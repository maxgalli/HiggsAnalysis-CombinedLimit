#include <RooFitHS3/RooJSONFactoryWSTool.h>
#include <RooFitHS3/JSONIO.h>
#include <RooFit/Detail/JSONInterface.h>
#include "static_execute.h"

#include "../interface/ProcessNormalization.h"

using RooFit::Detail::JSONNode;

class ProcessNormalizationStreamer : public RooFit::JSONIO::Exporter {
    public:
        std::string const &key() const override;
        bool exportObject(RooJSONFactoryWSTool *, const RooAbsArg *func, JSONNode &elem) const override
        {
            const ProcessNormalization *norm = static_cast<const ProcessNormalization *>(func);
            return true;
        }
};

#define DEFINE_EXPORTER_KEY(class_name, name)    \
   std::string const &class_name::key() const    \
   {                                             \
      const static std::string keystring = name; \
      return keystring;                          \
   }
DEFINE_EXPORTER_KEY(ProcessNormalizationStreamer, "polynomial_dist");

STATIC_EXECUTE([]() {
    using namespace RooFit::JSONIO;
    registerExporter<ProcessNormalizationStreamer>(ProcessNormalization::Class(), false);
});