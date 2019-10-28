#include <mbgl/storage/file_source.hpp>
#include <mbgl/style/conversion/geojson.hpp>
#include <mbgl/style/conversion/json.hpp>
#include <mbgl/style/layer.hpp>
#include <mbgl/style/source_observer.hpp>
#include <mbgl/style/sources/geojson_source.hpp>
#include <mbgl/style/sources/geojson_source_impl.hpp>
#include <mbgl/tile/tile.hpp>
#include <mbgl/util/logging.hpp>
#include <mbgl/util/thread_pool.hpp>

namespace mbgl {
namespace style {

GeoJSONSource::GeoJSONSource(const std::string& id, optional<GeoJSONOptions> options)
    : Source(makeMutable<Impl>(id, options)) {}

GeoJSONSource::~GeoJSONSource() = default;

const GeoJSONSource::Impl& GeoJSONSource::impl() const {
    return static_cast<const Impl&>(*baseImpl);
}

void GeoJSONSource::setURL(const std::string& url_) {
    url = url_;

    // Signal that the source description needs a reload
    if (loaded || req) {
        loaded = false;
        req.reset();
        backgroundParser.reset();
        observer->onSourceDescriptionChanged(*this);
    }
}

void GeoJSONSource::setGeoJSON(const mapbox::geojson::geojson& geoJSON) {
    req.reset();
    backgroundParser.reset();
    baseImpl = makeMutable<Impl>(impl(), geoJSON);
    observer->onSourceChanged(*this);
}

optional<std::string> GeoJSONSource::getURL() const {
    return url;
}

void GeoJSONSource::loadDescription(FileSource& fileSource) {
    if (!url) {
        loaded = true;
        return;
    }

    if (req) {
        return;
    }

    auto onResult = [this](Immutable<Source::Impl> result) {
        baseImpl = std::move(result);
        loaded = true;
        observer->onSourceLoaded(*this);
    };

    req = fileSource.request(Resource::source(*url), [this, onResult](Response res) {
        if (res.error) {
            observer->onSourceError(
                *this, std::make_exception_ptr(std::runtime_error(res.error->message)));
        } else if (res.notModified) {
            return;
        } else if (res.noContent) {
            observer->onSourceError(
                *this, std::make_exception_ptr(std::runtime_error("unexpectedly empty GeoJSON")));
        } else {
            auto makeImpl = [currentImpl = baseImpl, data = res.data]() -> Immutable<Source::Impl> {
                assert(data);
                auto& impl = static_cast<const Impl&>(*currentImpl);
                conversion::Error error;
                optional<GeoJSON> geoJSON = conversion::convertJSON<GeoJSON>(*data, error);
                if (!geoJSON) {
                    Log::Error(Event::ParseStyle, "Failed to parse GeoJSON data: %s", error.message.c_str());
                    // Create an empty GeoJSON VT object to make sure we're not infinitely waiting for
                    // tiles to load.
                    return makeMutable<Impl>(impl, GeoJSON{FeatureCollection{}});
                }
                return makeMutable<Impl>(impl, *geoJSON);
            };

            backgroundParser = std::make_unique<SequencedScheduler>();
            backgroundParser->scheduleAndReply(makeImpl, onResult);
        }
    });
}

bool GeoJSONSource::supportsLayerType(const mbgl::style::LayerTypeInfo* info) const {
    return mbgl::underlying_type(Tile::Kind::Geometry) == mbgl::underlying_type(info->tileKind);
}

} // namespace style
} // namespace mbgl
