#include "cover_flow_layout.hpp"
#include "library_logic.hpp"
#include <cassert>
#include <iostream>
int main() {
    assert(coverFlowMove(0, 0, 1) == 0);
    assert(coverFlowMove(0, 10, -1) == 0 && coverFlowMove(9, 10, 1) == 9);
    assert(coverFlowMove(5, 10, 1) == 6 && coverFlowMove(5, 10, -1) == 4);
    assert(coverFlowMove(1000, 10, 0) == 9);
    for (float width : {956.0f, 1108.0f}) for (float height : {300.0f, 374.0f, 420.0f}) {
        const auto art = coverFlowArtSize(width, height);
        const float centerY = 12 + art / 2;
        assert(art >= 48 && art <= 260);
        assert(centerY + art / 2 + 5 + art * 0.2f < height - 78);
        for (float distance : {-3.0f, -2.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f, 3.0f}) {
            const auto pose = coverFlowPose(distance, art), mirror = coverFlowPose(-distance, art);
            assert(pose.scaleX >= 0.5f && pose.scaleX <= 1 && pose.scaleY >= 0.88f && pose.scaleY <= 1);
            assert(pose.alpha > 0 && pose.alpha <= 1);
            assert(std::abs(pose.center + mirror.center) < 0.001f && std::abs(pose.shear + mirror.shear) < 0.001f);
            assert(std::abs(pose.center) + art * pose.scaleX / 2 < width / 2);
            assert(coverFlowHit(pose.center, centerY, centerY, art, pose));
            const float localX = art * 0.4f, localY = art * 0.4f;
            assert(coverFlowHit(pose.center + pose.scaleX * localX, centerY + pose.shear * localX + pose.scaleY * localY, centerY, art, pose));
            assert(!coverFlowHit(pose.center + pose.scaleX * art, centerY, centerY, art, pose));
            assert(!coverFlowHit(pose.center, centerY + art, centerY, art, pose));
        }
    }
    for (size_t count : {size_t{0}, size_t{1}, size_t{3}, size_t{50}}) {
        for (size_t selected = 0; selected < std::max(size_t{1}, count); ++selected) {
            for (float position : {static_cast<float>(selected), static_cast<float>(selected) - 0.5f}) {
                const auto order = coverFlowDrawOrder(selected, count, position);
                float previous = 1e9f; size_t valid = 0;
                for (size_t slot = 0; slot < order.size(); ++slot) {
                    const auto index = order[slot]; if (index >= count) continue;
                    const float distance = std::abs(index - position);
                    assert(distance <= previous); previous = distance; ++valid;
                    assert(std::abs(static_cast<long long>(index) - static_cast<long long>(selected)) <= CoverFlowRadius);
                    for (size_t other = slot + 1; other < order.size(); ++other) assert(order[other] != index);
                }
                const auto low = selected > CoverFlowRadius ? selected - CoverFlowRadius : 0;
                const auto high = std::min(count, selected + CoverFlowRadius + 1);
                assert(valid == high - low);
            }
        }
    }
    for (auto mode : {LibraryMode::Home, LibraryMode::Albums, LibraryMode::Artists, LibraryMode::Songs,
            LibraryMode::Favorites, LibraryMode::ArtistAlbums, LibraryMode::AlbumTracks, LibraryMode::Playlists, LibraryMode::PlaylistTracks}) {
        assert(libraryViewSlot(mode, true) < 10 && libraryViewSlot(mode, false) < 10);
    }
    assert(libraryViewSlot(LibraryMode::Favorites, true) != libraryViewSlot(LibraryMode::Favorites, false));
    std::cout << "Cover Flow sizing, reflection bounds, angled-cover hit tests and screen slots passed\n";
}
