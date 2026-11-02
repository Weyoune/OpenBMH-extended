// SPDX-License-Identifier: GPL-3.0-only
#include "ui/flow.hpp"
namespace bmh::ui {
FlowResult StartupFlow::finish_world12_market_input(PreparationRequest action,
                                                    bool changed) {
    if (world12_market_pending_)
        return {};
    World12MarketRequest request;
    request.identity = *world12_market_identity_;
    if (const auto* value = std::get_if<TransferMarketRequest>(&action))
        request.input = *value;
    else if (const auto* value = std::get_if<OutgoingTransferRequest>(&action))
        request.input = *value;
    else if (transfer_market_ && transfer_market_->close_requested() &&
             !outgoing_transfer_)
        request.close = true;
    else
        return {changed};
    world12_market_pending_ = request;
    FlowResult out{changed, FlowRequest::world12_market_action};
    out.world12_market_action = std::move(request);
    return out;
}
void StartupFlow::show_world12_market(World12MarketState page,
                                      PreparationState parent,
                                      const DeskRequest& request) {
    if (!world12_reference_ || !desktop_ ||
        desktop_->state().source != "world12" || world12_market_identity_ ||
        in_preparation_menu() || in_readonly_view() || !desktop_->pending() ||
        *desktop_->pending() != request ||
        request.kind != DeskRequestKind::open_menu || request.menu_kind != 17 ||
        request.manager_slot != desktop_->state().manager_slot ||
        request.revision != desktop_->state().revision ||
        page.identity.revision != request.revision + 1 ||
        parent.desktop.revision != page.identity.revision || page.offer ||
        page.identity.club != desktop_->state()
                                  .managers.at(size_t(request.manager_slot))
                                  .world_club)
        throw FormatError(
            "world12: market opening has no matching desktop owner");
    auto before = desktop_->state();
    before.revision = parent.desktop.revision;
    if (desk_json(before) != desk_json(parent.desktop))
        throw FormatError(
            "world12: market entry changed unrelated desktop fields");
    parent.transfer_market = page.market;
    validate_preparation_context(parent);
    World12MarketView next;
    next.identity = page.identity;
    next.market = std::make_unique<TransferMarketScreen>(
        data_, std::move(page.market), render_reference());
    next.desktop = std::make_unique<DesktopScreen>(data_, parent.desktop,
                                                   render_reference());
    next.preparation = std::make_unique<PreparationState>(std::move(parent));
    finish_world12_market(std::move(next));
}
StartupFlow::World12MarketView StartupFlow::prepare_world12_market_result(
    World12MarketState page, PreparationState parent,
    const World12MarketRequest& request, bool closed) const {
    if (!world12_market_pending_ || *world12_market_pending_ != request ||
        !world12_market_identity_ ||
        *world12_market_identity_ != request.identity || !transfer_market_ ||
        !desktop_ || !world12_reference_ || closed != request.close)
        throw FormatError(
            "world12: market result has no current owned request");
    auto expected = request.identity;
    ++expected.revision;
    ++expected.sequence;
    if (page.identity != expected ||
        parent.desktop.revision != expected.revision)
        throw FormatError("world12: market result revision/sequence is stale");
    if (parent.desktop.source != "world12" ||
        parent.desktop.manager_slot != desktop_->state().manager_slot ||
        parent.desktop.managers != desktop_->state().managers ||
        parent.desktop.day_counter != desktop_->state().day_counter ||
        parent.desktop.options != desktop_->state().options)
        throw FormatError(
            "world12: market result changed its manager/calendar context");
    parent.transfer_market = page.market;
    validate_preparation_context(parent);
    World12MarketView next;
    if (!closed) {
        next.identity = page.identity;
        next.market = std::make_unique<TransferMarketScreen>(*transfer_market_);
        if (const auto* market =
                std::get_if<TransferMarketRequest>(&request.input)) {
            if (market->kind == TransferMarketRequestKind::preview_quote) {
                if (!page.preview)
                    throw FormatError(
                        "world12: Source preview result is missing");
                next.market->acknowledge_preview(page.market, *page.preview);
            } else
                next.market->acknowledge(page.market, page.notice);
        } else if (std::holds_alternative<OutgoingTransferRequest>(
                       request.input))
            next.market->refresh_after_offer(page.market);
        if (page.offer) {
            if (outgoing_transfer_) {
                next.offer = std::make_unique<OutgoingTransferScreen>(
                    *outgoing_transfer_);
                next.offer->acknowledge(*page.offer);
            } else
                next.offer = std::make_unique<OutgoingTransferScreen>(
                    data_, *page.offer, render_reference());
        }
    }
    next.desktop = std::make_unique<DesktopScreen>(data_, parent.desktop,
                                                   render_reference());
    next.preparation = std::make_unique<PreparationState>(std::move(parent));
    return next;
}
void StartupFlow::finish_world12_market(World12MarketView next) noexcept {
    desktop_ = std::move(next.desktop);
    preparation_ = std::move(next.preparation);
    transfer_market_ = std::move(next.market);
    outgoing_transfer_ = std::move(next.offer);
    world12_market_identity_ = next.identity;
    world12_market_pending_.reset();
}
void StartupFlow::reject_world12_market(std::string reason) {
    if (!world12_market_pending_ || reason.empty())
        throw FormatError("world12: no market action to reject");
    if (world12_market_pending_->close)
        transfer_market_->reject_close(reason);
    else if (std::holds_alternative<TransferMarketRequest>(
                 world12_market_pending_->input))
        transfer_market_->reject_action(reason);
    else
        outgoing_transfer_->reject_action(reason);
    world12_market_pending_.reset();
}
} // namespace bmh::ui
