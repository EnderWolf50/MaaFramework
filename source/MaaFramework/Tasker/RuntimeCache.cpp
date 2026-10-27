#include "RuntimeCache.h"

#include <mutex>
#include <set>
#include <tuple>

#include "Global/OptionMgr.h"
#include "MaaUtils/Logger.h"

MAA_NS_BEGIN

std::optional<MaaNodeId> RuntimeCache::get_latest_node(const std::string& name) const
{
    if (name.empty()) {
        LogWarn << "name is empty";
        return std::nullopt;
    }

    std::shared_lock lock(latest_nodes_mutex_);

    auto it = latest_nodes_.find(name);
    if (it == latest_nodes_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void RuntimeCache::set_latest_node(const std::string& name, MaaNodeId id)
{
    if (name.empty() || id == MaaInvalidId) {
        LogError << "name is empty or id is invalid" << VAR(name) << VAR(id);
        return;
    }

    std::optional<RetainedNode> stale;
    {
        std::unique_lock lock(latest_nodes_mutex_);

        latest_nodes_.insert_or_assign(name, id);

        auto it = retained_nodes_.find(name);
        if (it != retained_nodes_.end() && it->second.node_id != id) {
            stale = it->second;
            retained_nodes_.erase(it);
        }
    }
    if (stale) {
        erase_node_and_reco(stale->node_id, stale->reco_id);
    }
}

std::optional<MAA_TASK_NS::RecoResult> RuntimeCache::get_reco_result(MaaRecoId uid) const
{
    if (uid == MaaInvalidId) {
        LogWarn << "uid is invalid";
        return std::nullopt;
    }

    std::shared_lock lock(reco_details_mutex_);

    auto it = reco_details_.find(uid);
    if (it == reco_details_.end()) {
        return std::nullopt;
    }

    MAA_TASK_NS::RecoResult result = it->second;

    auto cache_it = reco_image_cache_.find(uid);
    if (cache_it != reco_image_cache_.end()) {
        result.raw = cache_it->second.raw;
        result.draws = cache_it->second.draws;
    }

    return result;
}

void RuntimeCache::set_reco_detail(MaaRecoId uid, MAA_TASK_NS::RecoResult detail)
{
    if (uid == MaaInvalidId) {
        LogError << "uid is invalid";
        return;
    }
    {
        std::lock_guard owner_lock(owner_mutex_);
        unowned_.recos.emplace_back(uid);
    }

    std::unique_lock lock(reco_details_mutex_);

    size_t limit = MAA_GLOBAL_NS::OptionMgr::get_instance().reco_image_cache_limit();

    // limit > 0 means caching is enabled
    if (limit > 0) {
        evict_reco_image_cache_if_needed(limit);

        reco_image_cache_.insert_or_assign(uid, RecoImageCache { std::move(detail.raw), std::move(detail.draws) });
        reco_image_order_.push_back(uid);
    }

    detail.raw.clear();
    detail.draws.clear();

    reco_details_.insert_or_assign(uid, std::move(detail));
}

void RuntimeCache::evict_reco_image_cache_if_needed(size_t limit)
{
    while (reco_image_cache_.size() >= limit && !reco_image_order_.empty()) {
        MaaRecoId oldest = reco_image_order_.front();
        reco_image_order_.pop_front();
        reco_image_cache_.erase(oldest);
    }
}

std::optional<MAA_TASK_NS::ActionResult> RuntimeCache::get_action_result(MaaActId uid) const
{
    if (uid == MaaInvalidId) {
        LogWarn << "uid is invalid";
        return std::nullopt;
    }

    std::shared_lock lock(action_details_mutex_);

    auto it = action_details_.find(uid);
    if (it == action_details_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void RuntimeCache::set_action_detail(MaaActId uid, MAA_TASK_NS::ActionResult detail)
{
    if (uid == MaaInvalidId) {
        LogError << "uid is invalid";
        return;
    }
    {
        std::lock_guard owner_lock(owner_mutex_);
        unowned_.actions.emplace_back(uid);
    }

    std::unique_lock lock(action_details_mutex_);

    action_details_.insert_or_assign(uid, std::move(detail));
}

std::optional<MAA_TASK_NS::WaitFreezesDetail> RuntimeCache::get_wf_detail(MaaWfId uid) const
{
    if (uid == MaaInvalidId) {
        LogWarn << "uid is invalid";
        return std::nullopt;
    }

    std::shared_lock lock(wf_details_mutex_);

    auto it = wf_details_.find(uid);
    if (it == wf_details_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void RuntimeCache::set_wf_detail(MaaWfId uid, MAA_TASK_NS::WaitFreezesDetail detail)
{
    if (uid == MaaInvalidId) {
        LogError << "uid is invalid";
        return;
    }
    {
        std::lock_guard owner_lock(owner_mutex_);
        unowned_.wfs.emplace_back(uid);
    }

    std::unique_lock lock(wf_details_mutex_);

    wf_details_.insert_or_assign(uid, std::move(detail));
}

std::optional<MAA_TASK_NS::NodeDetail> RuntimeCache::get_node_detail(MaaNodeId uid) const
{
    if (uid == MaaInvalidId) {
        LogWarn << "uid is invalid";
        return std::nullopt;
    }

    std::shared_lock lock(node_details_mutex_);

    auto it = node_details_.find(uid);
    if (it == node_details_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void RuntimeCache::set_node_detail(MaaNodeId uid, MAA_TASK_NS::NodeDetail detail)
{
    if (uid == MaaInvalidId) {
        LogError << "uid is invalid";
        return;
    }
    {
        std::lock_guard owner_lock(owner_mutex_);
        unowned_.nodes.emplace_back(uid);
    }

    std::unique_lock lock(node_details_mutex_);

    node_details_.insert_or_assign(uid, std::move(detail));
}

std::optional<MAA_TASK_NS::TaskDetail> RuntimeCache::get_task_detail(MaaTaskId uid) const
{
    if (uid == MaaInvalidId) {
        LogWarn << "uid is invalid";
        return std::nullopt;
    }

    std::shared_lock lock(task_details_mutex_);

    auto it = task_details_.find(uid);
    if (it == task_details_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void RuntimeCache::set_task_detail(MaaTaskId uid, MAA_TASK_NS::TaskDetail detail)
{
    if (uid == MaaInvalidId) {
        LogError << "uid is invalid";
        return;
    }

    std::unique_lock lock(task_details_mutex_);

    task_details_.insert_or_assign(uid, std::move(detail));
}

void RuntimeCache::own_task_node(MaaTaskId task_id, MaaNodeId node_id)
{
    std::optional<std::pair<MaaNodeId, OwnedIds>> dropped;
    {
        std::lock_guard owner_lock(owner_mutex_);

        auto& nodes = task_nodes_[task_id];
        nodes.emplace_back(node_id, std::exchange(unowned_, OwnedIds { }));
        if (nodes.size() > kTaskNodeLimit) {
            dropped = std::move(nodes.front());
            nodes.pop_front();
        }
    }
    if (!dropped) {
        return;
    }

    erase_owned(dropped->second);

    std::unique_lock lock(task_details_mutex_);
    if (auto it = task_details_.find(task_id); it != task_details_.end()) {
        std::erase(it->second.node_ids, dropped->first);
    }
}

void RuntimeCache::own_subtask(MaaTaskId subtask_id)
{
    std::lock_guard owner_lock(owner_mutex_);
    unowned_.subtasks.emplace_back(subtask_id);
}

void RuntimeCache::erase_owned(const OwnedIds& ids)
{
    std::vector<std::tuple<std::string, MaaNodeId, MaaRecoId>> named;
    {
        std::shared_lock lock(node_details_mutex_);
        for (MaaNodeId id : ids.nodes) {
            if (auto it = node_details_.find(id); it != node_details_.end() && !it->second.name.empty()) {
                named.emplace_back(it->second.name, id, it->second.reco_id);
            }
        }
    }

    std::set<MaaNodeId> kept_nodes;
    std::set<MaaRecoId> kept_recos;
    {
        std::unique_lock lock(latest_nodes_mutex_);
        for (const auto& [name, node_id, reco_id] : named) {
            auto it = latest_nodes_.find(name);
            if (it == latest_nodes_.end() || it->second != node_id) {
                continue;
            }
            retained_nodes_.insert_or_assign(name, RetainedNode { .node_id = node_id, .reco_id = reco_id });
            kept_nodes.emplace(node_id);
            kept_recos.emplace(reco_id);
        }
    }

    {
        std::unique_lock lock(node_details_mutex_);
        for (MaaNodeId id : ids.nodes) {
            if (!kept_nodes.contains(id)) {
                node_details_.erase(id);
            }
        }
    }
    {
        // the encoded images stay in reco_image_cache_, which has its own limit
        std::unique_lock lock(reco_details_mutex_);
        for (MaaRecoId id : ids.recos) {
            if (!kept_recos.contains(id)) {
                reco_details_.erase(id);
            }
        }
    }
    {
        std::unique_lock lock(action_details_mutex_);
        for (MaaActId id : ids.actions) {
            action_details_.erase(id);
        }
    }
    {
        std::unique_lock lock(wf_details_mutex_);
        for (MaaWfId id : ids.wfs) {
            wf_details_.erase(id);
        }
    }
    {
        std::unique_lock lock(task_details_mutex_);
        for (MaaTaskId id : ids.subtasks) {
            task_details_.erase(id);
        }
    }
}

void RuntimeCache::erase_node_and_reco(MaaNodeId node_id, MaaRecoId reco_id)
{
    {
        std::unique_lock lock(node_details_mutex_);
        node_details_.erase(node_id);
    }
    std::unique_lock lock(reco_details_mutex_);
    reco_details_.erase(reco_id);
}

void RuntimeCache::clear()
{
    LogInfo;

    {
        std::unique_lock lock(latest_nodes_mutex_);
        latest_nodes_.clear();
        retained_nodes_.clear();
    }
    {
        std::lock_guard owner_lock(owner_mutex_);
        unowned_ = { };
        task_nodes_.clear();
    }
    {
        std::unique_lock lock(reco_details_mutex_);
        reco_details_.clear();
        reco_image_cache_.clear();
        reco_image_order_.clear();
    }
    {
        std::unique_lock lock(action_details_mutex_);
        action_details_.clear();
    }
    {
        std::unique_lock lock(wf_details_mutex_);
        wf_details_.clear();
    }
    {
        std::unique_lock lock(node_details_mutex_);
        node_details_.clear();
    }
    {
        std::unique_lock lock(task_details_mutex_);
        task_details_.clear();
    }
}

MAA_NS_END
