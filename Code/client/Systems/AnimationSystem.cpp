#include <TiltedOnlinePCH.h>

#include <Systems/AnimationSystem.h>

#include <Games/Animation/TESActionData.h>
#include <Games/Animation/ActorMediator.h>

#include <Games/References.h>

#include <Forms/BGSAction.h>
#include <AI/AIProcess.h>
#include <Camera/PlayerCamera.h>
#include <Misc/MiddleProcess.h>

#include <Messages/RequestActionEvents.h>
#include <Utils.h>
#include <WireProtocol.h>
#include <Structs/Movement.h>

#include <Components.h>
#include <World.h>

#include <Forms/TESObjectCELL.h>
#include <Forms/TESWorldSpace.h>

void AnimationSystem::Update(World& aWorld, Actor* apActor, RemoteAnimationComponent& aAnimationComponent, const uint64_t aTick) noexcept
{
    auto& actions = aAnimationComponent.TimePoints;

    // A pending reset does not wait for an action: a replay chain that was only an exit action arrives empty,
    // and the actor may still be in the state this client loaded it in (e.g. sitting).
    if (aAnimationComponent.ResetAnimationGraphForReplay)
    {
        if (!apActor->animationGraphHolder.IsReady())
            return;

        spdlog::info("Reverting the animation graph of remote actor {:X} before replaying {} actions", apActor->formID, aAnimationComponent.ReplayCount);

        apActor->animationGraphHolder.RevertAnimationGraphManager();
        apActor->actorState.flags1 &= ~ActorState::kSitSleepStateMask;
        aAnimationComponent.ResetAnimationGraphForReplay = false;
    }

    const auto it = std::begin(actions);
    if (it != std::end(actions) && it->Tick <= aTick)
    {
        // Check if animation graph is ready before attempting to play animations
        if (!apActor->animationGraphHolder.IsReady())
        {
            // Animation graph not ready, keep the action in queue and try again later
            return;
        }

        const auto& first = *it;

        const auto actionId = first.ActionId;
        const auto targetId = first.TargetId;

        const auto pAction = Cast<BGSAction>(TESForm::GetById(actionId));
        // An actor the owner targets is found by its server id: its form ID here is another actor (or this player)
        const auto pTarget = (first.Type & ActionEvent::kTargetIsServerId) ? Utils::GetByServerId<TESObjectREFR>(targetId)
                                                                          : Cast<TESObjectREFR>(TESForm::GetById(targetId));

        apActor->actorState.flags1 = first.State1;
        apActor->actorState.flags2 = first.State2;

        apActor->LoadAnimationVariables(first.Variables);

        aAnimationComponent.LastRanAction = first;

        // Play the animation
        TESActionData actionData(first.Type & 0x3, apActor, pAction, pTarget);
        actionData.eventName = BSFixedString(first.EventName.c_str());
        actionData.idleForm = Cast<TESIdleForm>(TESForm::GetById(first.IdleId));
        actionData.someFlag = ((first.Type & 0x4) != 0) ? 1 : 0;

        const auto result = ActorMediator::Get()->ForceAction(&actionData);

        if (aAnimationComponent.ReplayCount > 0)
            aAnimationComponent.ReplayCount--;

        actions.pop_front();
    }
}

void AnimationSystem::Setup(World& aWorld, const entt::entity aEntity) noexcept
{
    aWorld.emplace_or_replace<RemoteAnimationComponent>(aEntity);
}

void AnimationSystem::Clean(World& aWorld, const entt::entity aEntity) noexcept
{
    if (aWorld.all_of<RemoteAnimationComponent>(aEntity))
        aWorld.remove<RemoteAnimationComponent>(aEntity);
}

void AnimationSystem::AddActionsForReplay(RemoteAnimationComponent& aAnimationComponent,
                                          const ActionReplayChain& acReplay, const Actor* apActor) noexcept
{
    // A replay rebuilds past state, so it is due right away rather than when playback reaches the original ticks
    for (auto action : acReplay.Actions)
    {
        action.Tick = 0;
        aAnimationComponent.TimePoints.push_back(std::move(action));
    }

    aAnimationComponent.ReplayCount = acReplay.Actions.size();
    aAnimationComponent.ResetAnimationGraphForReplay = acReplay.ResetAnimationGraph;

    // This client may have loaded the actor sitting or sleeping through its own AI package while the owner has it
    // standing: the chain then has no exit action and the actor would keep its seated graph while moving around.
    // The owner's state rides on every action, so the latest one tells whether the local pose is stale. The reset
    // runs before the replay, so an owner that sat down afterwards still ends up seated.
    if (apActor && !acReplay.ResetAnimationGraph && !acReplay.Actions.empty() && apActor->actorState.IsSittingOrSleeping() &&
        !ActorState::IsSittingOrSleeping(acReplay.Actions.back().State1))
    {
        spdlog::info("Resetting the animation graph of remote actor {:X}: seated or asleep locally (state {:X}) but not for its owner (last action '{}')", apActor->formID,
                     (apActor->actorState.flags1 & ActorState::kSitSleepStateMask) >> 14, acReplay.Actions.back().EventName.c_str());
        aAnimationComponent.ResetAnimationGraphForReplay = true;
    }
}

void AnimationSystem::AddAction(RemoteAnimationComponent& aAnimationComponent, const std::string& acActionDiff) noexcept
{
    auto itor = std::begin(aAnimationComponent.TimePoints);
    const auto end = std::cend(aAnimationComponent.TimePoints);

    auto& lastProcessedAction = aAnimationComponent.LastProcessedAction;

    TiltedPhoques::ViewBuffer buffer((uint8_t*)acActionDiff.data(), acActionDiff.size());
    Buffer::Reader reader(&buffer);

    lastProcessedAction.ApplyDifferential(reader);

    aAnimationComponent.TimePoints.push_back(lastProcessedAction);
}

void AnimationSystem::SerializeMovement(World& aWorld, Actor* apActor, Movement& aMovement) noexcept
{
    if (const auto pCell = apActor->GetParentCellEx())
        aWorld.GetModSystem().GetServerModId(pCell->formID, aMovement.CellId.ModId, aMovement.CellId.BaseId);

    if (const auto pWorldSpace = apActor->GetWorldSpace())
        aWorld.GetModSystem().GetServerModId(pWorldSpace->formID, aMovement.WorldSpaceId.ModId, aMovement.WorldSpaceId.BaseId);

    aMovement.Position = apActor->position;

    aMovement.Rotation.x = apActor->rotation.x;
    aMovement.Rotation.y = apActor->rotation.z;

    apActor->SaveAnimationVariables(aMovement.Variables);

    if (apActor->currentProcess && apActor->currentProcess->middleProcess)
    {
        aMovement.Direction = apActor->currentProcess->middleProcess->direction;

        static BSFixedString s_targetLocation("TargetLocation");
        NiPoint3 target{};

        if (apActor == PlayerCharacter::Get() && apActor->currentProcess->middleProcess->lookAt && PlayerCamera::Get()->IsThirdPerson() &&
            apActor->animationGraphHolder.GetVariableNiPoint3(&s_targetLocation, &target))
        {
            aMovement.HasLookAt = true;
            aMovement.LookOffset = target - apActor->GetLookingAtLocation();
        }
    }
}

void AnimationSystem::SerializeActions(World& aWorld, RequestActionEvents& aMessage, LocalComponent& localComponent, LocalAnimationComponent& animationComponent) noexcept
{
    if (animationComponent.Actions.empty())
        return;

    // Official v1.8.4 clients replay targets as form IDs
    if (!Wire::IsOfficial184())
    {
        const auto actors = aWorld.view<FormIdComponent>();
        for (ActionEvent& action : animationComponent.Actions)
        {
            if (!action.TargetId || (action.Type & ActionEvent::kTargetIsServerId))
                continue;
            const auto it = std::find_if(actors.begin(), actors.end(), [&](entt::entity aEntity)
                                         { return actors.get<FormIdComponent>(aEntity).Id == action.TargetId && aWorld.any_of<LocalComponent, RemoteComponent>(aEntity); });
            if (it == actors.end())
                continue;
            if (const std::optional<uint32_t> cServerId = Utils::GetServerId(*it))
            {
                action.TargetId = *cServerId;
                action.Type |= ActionEvent::kTargetIsServerId;
            }
        }
    }

    localComponent.CurrentAction = animationComponent.Actions.back();

    aMessage.Actions[localComponent.Id] = std::move(animationComponent.Actions);
    aMessage.OwnershipEpochs[localComponent.Id] = localComponent.OwnershipEpoch;
    animationComponent.Actions.clear();
}

bool AnimationSystem::Serialize(World& aWorld, const ActionEvent& aActionEvent, const ActionEvent& aLastProcessedAction, std::string* apData)
{
    uint32_t actionBaseId = 0;
    uint32_t actionModId = 0;
    if (!aWorld.GetModSystem().GetServerModId(aActionEvent.ActionId, actionModId, actionBaseId))
        return false;

    uint32_t targetBaseId = 0;
    uint32_t targetModId = 0;
    if (!aWorld.GetModSystem().GetServerModId(aActionEvent.TargetId, targetModId, targetBaseId))
        return false;

    uint8_t scratch[1 << 14];
    TiltedPhoques::ViewBuffer buffer(scratch, std::size(scratch));
    Buffer::Writer writer(&buffer);
    aActionEvent.GenerateDifferential(aLastProcessedAction, writer);

    apData->assign(buffer.GetData(), buffer.GetData() + writer.Size());

    return true;
}
