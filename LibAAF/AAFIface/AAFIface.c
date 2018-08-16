/*
 *	This file is part of LibAAF.
 *
 *	Copyright (c) 2017 Adrien Gesta-Fline
 *
 *	LibAAF is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU Affero General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	any later version.
 *
 *	LibAAF is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU Affero General Public License for more details.
 *
 *	You should have received a copy of the GNU Affero General Public License
 *	along with LibAAF. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 *	@file LibAAF/AAFIface/AAFIface.c
 *	@brief AAF processing
 *	@author Adrien Gesta-Fline
 *	@version 0.1
 *	@date 04 october 2017
 *
 *	@ingroup AAFIface
 *	@addtogroup AAFIface
 *
 *	The AAFIface provides the actual processing of the AAF Objects in order to show
 *	essences and clips in a simplified manner. Indeed, AAF has many different ways to
 *	store data and metadata. Thus, the AAFIface is an abstraction layer that provides
 *	a constant and unique representation method of essences and clips.
 *
 *
 *
 *	@{
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../libAAF.h"
#include "../common/debug.h"










AAF_Iface * aafi_alloc( AAF_Data *aafd )
{
	AAF_Iface *aafi = calloc( sizeof(AAF_Iface), sizeof(unsigned char) );

	if ( aafi == NULL )
	{
		_error( "%s.\n", strerror( errno ) );
		return NULL;
	}


	aafi->Audio = malloc( sizeof(aafiAudio) );

	if ( aafi->Audio == NULL )
	{
		_error( "%s.\n", strerror( errno ) );
		return NULL;
	}

	aafi->Audio->Essences = NULL;
	aafi->Audio->tc = NULL;
	aafi->Audio->Tracks = NULL;


	if ( aafd != NULL )
	{
		aafi->aafd = aafd;
	}
	else
	{
		aafi->aafd = aaf_alloc();
	}

	aafi->compositionName = NULL;

	return aafi;
}




void aafi_release( AAF_Iface **aafi )
{
	if ( *aafi == NULL )
		return;


	aaf_release( &(*aafi)->aafd );


	if ( (*aafi)->compositionName != NULL )
	{
		free( (*aafi)->compositionName );
	}


	if ( (*aafi)->Comments )
	{
		aafi_freeUserComments( &((*aafi)->Comments) );
	}


	if ( (*aafi)->Audio != NULL )
	{
		if ( (*aafi)->Audio->Tracks != NULL )
		{
			aafi_freeAudioTracks( &(*aafi)->Audio->Tracks );
		}

		if ( (*aafi)->Audio->Essences != NULL )
		{
			aafi_freeAudioEssences( &(*aafi)->Audio->Essences );
		}

		if ( (*aafi)->Audio->tc != NULL )
		{
			free( (*aafi)->Audio->tc );
		}

		free( (*aafi)->Audio );
	}


	free( *aafi );
}




int aafi_load_file( AAF_Iface *aafi, const char * file )
{
	if ( aaf_load_file( aafi->aafd, file ) )
	{
		return 1;
	}

	aafi_retrieveData( aafi );

	return 0;
}




aafiTransition * get_fadein( aafiTimelineItem *audioItem )
{

	if ( audioItem->prev != NULL &&
		 audioItem->prev->type & AAFI_TRANS )
	{
		aafiTransition *Trans = (aafiTransition*)(audioItem->prev->data);

		if ( Trans->flags & AAFI_TRANS_FADE_IN )
			return (aafiTransition*)(audioItem->prev->data);
	}

	return NULL;
}




aafiTransition * get_fadeout( aafiTimelineItem *audioItem )
{

	if ( audioItem->next != NULL &&
		 audioItem->next->type & AAFI_TRANS )
	{
		aafiTransition *Trans = (aafiTransition*)(audioItem->next->data);

		if ( Trans->flags & AAFI_TRANS_FADE_OUT )
			return (aafiTransition*)(audioItem->next->data);
	}

	return NULL;
}








aafiTimelineItem * aafi_newTimelineItem( aafiAudioTrack *track, int itemType )
{

	aafiTimelineItem *item = NULL;

	if ( itemType == AAFI_CLIP )
	{
		item = calloc( sizeof(aafiTimelineItem) + sizeof(aafiAudioClip),  1 );

		if ( item == NULL )
		{
			_error( "%s.\n", strerror( errno ) );
			return NULL;
		}


		item->type |= AAFI_CLIP;

		aafiAudioClip *audioClip = (aafiAudioClip*)&item->data;

		audioClip->track = track;
	}
	else if ( itemType == AAFI_TRANS )
	{
		item = calloc( sizeof(aafiTimelineItem) + sizeof(aafiTransition), 1 );

		if ( item == NULL )
		{
			_error( "%s.\n", strerror( errno ) );
			return NULL;
		}

		item->type |= AAFI_TRANS;
	}



	if ( track != NULL )
	{
		/*
		 *	Add to track's item list
		 */

		if ( track->Items != NULL )
		{
			aafiTimelineItem *tmp = track->Items;

			for (; tmp != NULL; tmp = tmp->next )
				if ( tmp->next == NULL )
					break;

			tmp->next  = item;
			item->prev = tmp;
		}
		else
		{
			track->Items = item;
			item->prev = NULL;
		}
	}


	return item;
}





void aafi_freeTimelineItem( aafiTimelineItem **item )
{

	if ( (*item)->type == AAFI_TRANS )
	{
		aafi_freeTransition( (aafiTransition*)&((*item)->data) );
	}
	else if ( (*item)->type == AAFI_CLIP )
	{
		aafiAudioClip *audioClip = (aafiAudioClip*)(*item)->data;

		if ( audioClip->gain != NULL )
		{
			if ( audioClip->gain->time != NULL )
			{
				free( audioClip->gain->time );
			}

			if ( audioClip->gain->value != NULL )
			{
				free( audioClip->gain->value );
			}

			free( audioClip->gain );
		}
	}

	free( *item );

	*item = NULL;
}




void aafi_freeTimelineItems( aafiTimelineItem **items )
{
	aafiTimelineItem *item = NULL;
	aafiTimelineItem *nextItem = NULL;

	for ( item = (*items); item != NULL; item = nextItem )
	{
		nextItem = item->next;

		aafi_freeTimelineItem( &item );
	}

	*items = NULL;
}








aafiUserComment * aafi_newUserComment( aafiUserComment **CommentList )
{

	aafiUserComment *UserComment = calloc( sizeof(aafiUserComment),  1 );

	if ( UserComment == NULL )
	{
		_error( "%s.\n", strerror( errno ) );
		return NULL;
	}


	if ( CommentList != NULL )
	{
		UserComment->next = *CommentList;
		*CommentList = UserComment;
	}
	else
	{
		*CommentList = UserComment;
	}


	return UserComment;
}




void aafi_freeUserComments( aafiUserComment **CommentList )
{
	aafiUserComment *UserComment = *CommentList;
	aafiUserComment *tmp = NULL;

	while( UserComment != NULL )
	{
		tmp = UserComment;
		UserComment = UserComment->next;

		if ( tmp->name != NULL )
		{
			free( tmp->name );
		}

		if ( tmp->text != NULL )
		{
			free( tmp->text );
		}

		free( tmp );
	}

	*CommentList = NULL;
}








void aafi_freeTransition( aafiTransition *Transition )
{
	if ( Transition->value_a != NULL )
	{
		free( Transition->value_a );
	}

	if ( Transition->value_b != NULL )
	{
		free( Transition->value_b );
	}

	if ( Transition->time_a != NULL )
	{
		free( Transition->time_a );
	}

	if ( Transition->time_b != NULL )
	{
		free( Transition->time_b );
	}
}








aafiAudioTrack * aafi_newAudioTrack( AAF_Iface *aafi )
{
	aafiAudioTrack *track = calloc( sizeof(aafiAudioTrack), sizeof(unsigned char) );

	if ( track == NULL )
	{
		_error( "%s.\n", strerror( errno ) );
		return NULL;
	}


	track->next = NULL;

	track->format = AAFI_TRACK_FORMAT_MONO;



	/*
	 *	Add to track list
	 */

	if ( aafi->Audio->Tracks != NULL )
	{
		aafiAudioTrack *tmp = aafi->Audio->Tracks;

		for (; tmp != NULL; tmp = tmp->next )
			if ( tmp->next == NULL )
				break;

		tmp->next = track;
	}
	else
	{
		aafi->Audio->Tracks = track;
	}


	track->Audio = aafi->Audio;

	aafi->ctx.current_track = track;


	return track;
}




void aafi_freeAudioTracks( aafiAudioTrack **tracks )
{
	if ( *(tracks) == NULL )
	{
		return;
	}

	aafiAudioTrack *track = NULL;
	aafiAudioTrack *nextTrack = NULL;

	for ( track = (*tracks); track != NULL; track = nextTrack )
	{
		nextTrack = track->next;

		if ( track->name != NULL )
		{
			free( track->name );
		}

		if ( track->gain != NULL )
		{
			free( track->gain->time );
			free( track->gain->value );
			free( track->gain );
		}

		if ( track->pan != NULL )
		{
			free( track->pan->time );
			free( track->pan->value );
			free( track->pan );
		}

		if ( track->Items != NULL )
		{
			aafi_freeTimelineItems( &(track->Items) );
		}

		free( track );
	}

	*tracks = NULL;
}








aafiAudioEssence * aafi_newAudioEssence( AAF_Iface *aafi )
{
	aafiAudioEssence * audioEssence = calloc( sizeof(aafiAudioEssence), sizeof(char) );

	if ( audioEssence == NULL )
	{
		_error( "%s.\n", strerror( errno ) );
		return NULL;
	}


	audioEssence->next = aafi->Audio->Essences;

	audioEssence->original_file = NULL;
	audioEssence->exported_file = NULL;
	audioEssence->file_name = NULL;
	audioEssence->unique_file_name = NULL;

	aafi->Audio->Essences = audioEssence;

	return audioEssence;
}




void aafi_freeAudioEssences( aafiAudioEssence **audioEssence )
{
	if ( *(audioEssence) == NULL )
	{
		return;
	}

	// aafiAudioEssence *audioEssence = NULL;
	aafiAudioEssence *nextAudioEssence = NULL;

	for (; (*audioEssence) != NULL; *audioEssence = nextAudioEssence )
	{
		nextAudioEssence = (*audioEssence)->next;

		if ( (*audioEssence)->original_file != NULL )
		{
			free( (*audioEssence)->original_file );
		}

		if ( (*audioEssence)->exported_file != NULL )
		{
			free( (*audioEssence)->exported_file );
		}

		if ( (*audioEssence)->file_name != NULL )
		{
			free( (*audioEssence)->file_name );
		}

		if ( (*audioEssence)->unique_file_name != NULL )
		{
			free( (*audioEssence)->unique_file_name );
		}

		free( *audioEssence );
	}

	*audioEssence = NULL;
}



/**
 *	@}
 */
