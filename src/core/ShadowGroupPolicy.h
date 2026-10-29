#ifndef SHADOW_GROUP_POLICY_H
#define SHADOW_GROUP_POLICY_H
#include "Group.h"
namespace ShadowGroupPolicy {
inline bool canChange( const Group& current, const Group& proposed, VNumber actorId, const QString& actorHash )
{
  if( !current.hasUser( actorId ) || current.ownerHash().isEmpty() ) return false;
  if( actorHash == current.ownerHash() ) return true;
  // Ordinary members can invite, but cannot rename, remove, or transfer.
  return current.name() == proposed.name() && current.ownerHash() == proposed.ownerHash() &&
         proposed.hasUsers( current.usersId() );
}
}
#endif
